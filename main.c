#include "xparameters.h"
#include "xiic.h"
#include "xil_printf.h"
#include "sleep.h"

#define IIC_DEVICE_ID XPAR_AXI_IIC_0_DEVICE_ID
#define ADAU1761_ADDR 0x3B

XIic Iic;

// ================= I2C INIT =================
int init_iic()
{
    int status;

    status = XIic_Initialize(&Iic, IIC_DEVICE_ID);
    if (status != XST_SUCCESS)
    {
        xil_printf("IIC Init Failed\r\n");
        return XST_FAILURE;
    }

    xil_printf("IIC Init OK\r\n");
    return XST_SUCCESS;
}

// ================= WRITE =================
int write_adau1761(u16 reg, u64 data, int nbytes)
{
    u8 buf[10];
    int i;

    buf[0] = (reg >> 8) & 0xFF;
    buf[1] = reg & 0xFF;

    for (i = 0; i < nbytes; i++)
    {
        buf[2 + i] = (data >> (8*(nbytes-1-i))) & 0xFF;
    }

    int sent = XIic_Send(Iic.BaseAddress,
                         ADAU1761_ADDR,
                         buf,
                         nbytes + 2,
                         XIIC_STOP);

    if (sent != (nbytes + 2))
    {
        xil_printf("WRITE FAIL 0x%04x\r\n", reg);
        return XST_FAILURE;
    }

    xil_printf("WRITE OK 0x%04x\r\n", reg);
    usleep(1000);

    return XST_SUCCESS;
}

// ================= READ =================
int read_adau1761(u16 reg, int nbytes, u64 *out)
{
    u8 addr_buf[2];
    u8 data_buf[8];
    u64 result = 0;
    int i;

    addr_buf[0] = (reg >> 8) & 0xFF;
    addr_buf[1] = reg & 0xFF;

    xil_printf("Start READ 0x%04x\r\n", reg);

    // STEP 1: WRITE register address (NO STOP)
    int sent = XIic_Send(Iic.BaseAddress,
                         ADAU1761_ADDR,
                         addr_buf,
                         2,
                         XIIC_REPEATED_START);

    if (sent != 2)
    {
        xil_printf("READ ADDR FAIL\r\n");
        return XST_FAILURE;
    }

    // STEP 2: READ data
    int recv = XIic_Recv(Iic.BaseAddress,
                         ADAU1761_ADDR,
                         data_buf,
                         nbytes,
                         XIIC_STOP);

    if (recv != nbytes)
    {
        xil_printf("READ DATA FAIL\r\n");
        return XST_FAILURE;
    }

    for (i = 0; i < nbytes; i++)
    {
        result = (result << 8) | data_buf[i];
    }

    xil_printf("READ 0x%04x = 0x%llx\r\n", reg, result);

    *out = result;
    return XST_SUCCESS;
}

// ================= RESET =================
void reset_adau1761()
{
    u64 tmp;

    xil_printf("Reset ADAU1761...\r\n");

    read_adau1761(0x0000,1,&tmp);
    read_adau1761(0x0000,1,&tmp);
    read_adau1761(0x0000,1,&tmp);
}

// ================= INIT ADAU =================
void init_adau1761()
{
    volatile int i;
    int timeout = 100000;
    u64 val;

    xil_printf("Config ADAU1761...\r\n");

    write_adau1761(0x4000,0x0e,1);
    write_adau1761(0x4002,0x007d000c2301,6);

    // Wait PLL lock
    while (timeout--)
    {
        if (read_adau1761(0x4002,6,&val) != XST_SUCCESS)
        {
            xil_printf("READ ERROR\r\n");
            break;
        }

        if (val & 0x02)
        {
            xil_printf("PLL LOCK OK\r\n");
            break;
        }
    }

    if (timeout == 0)
    {
        xil_printf("PLL LOCK FAIL\r\n");
    }

    write_adau1761(0x4000,0x0f,1);

    for (i=0; i<1000000; i++);

    write_adau1761(0x4015,0x01,1);

    write_adau1761(0x400a,0x01,1);
    write_adau1761(0x400b,0x05,1);
    write_adau1761(0x400c,0x01,1);
    write_adau1761(0x400d,0x05,1);

    write_adau1761(0x401c,0x21,1);
    write_adau1761(0x401e,0x41,1);

    write_adau1761(0x4023,0xe7,1);
    write_adau1761(0x4024,0xe7,1);
    write_adau1761(0x4025,0xe7,1);
    write_adau1761(0x4026,0xe7,1);

    write_adau1761(0x4019,0x03,1);
    write_adau1761(0x4029,0x03,1);
    write_adau1761(0x402a,0x03,1);

    write_adau1761(0x40f2,0x01,1);
    write_adau1761(0x40f3,0x01,1);

    write_adau1761(0x40f9,0x7f,1);
    write_adau1761(0x40fa,0x03,1);

    xil_printf("\r\n===== VERIFY =====\r\n");

    read_adau1761(0x4000,1,&val);
    read_adau1761(0x4015,1,&val);
}

// ================= MAIN =================
int main()
{
    xil_printf("===== START =====\r\n");

    init_iic();

    // Test device ACK
    u8 test = 0x00;
    int ret = XIic_Send(Iic.BaseAddress, ADAU1761_ADDR, &test, 1, XIIC_STOP);

    if (ret == 0)
    {
        xil_printf("NO I2C DEVICE!\r\n");
        while(1);
    }

    xil_printf("I2C OK\r\n");

    reset_adau1761();

    init_adau1761();

    xil_printf("===== DONE =====\r\n");

    while(1);
}
