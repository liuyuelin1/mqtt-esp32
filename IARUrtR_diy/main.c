#include"init.h"
#include"business_cmd.h"
main()
{
    InitUART1();
    Init_Timer4();
    GpioInit();
    ReadData();
    StatusInit();
    WDogInit();
    asm("rim");//打开全局中断
    while (1){
        CheckUARTCmd();
        AnalysysCmd();
        ExeCmd();
    }
}
