#include "debug.h"

#define LCD_CS_CLR() GPIOB->BSHR = GPIO_BSHR_BR12
#define LCD_DC_CLR() GPIOC->BSHR = GPIO_BSHR_BR6
#define LCD_RST_CLR() GPIOC->BSHR = GPIO_BSHR_BR7
#define LCD_CS_SET() GPIOB->BSHR = GPIO_BSHR_BS12
#define LCD_DC_SET() GPIOC->BSHR = GPIO_BSHR_BS6
#define LCD_RST_SET() GPIOC->BSHR = GPIO_BSHR_BS7

static void LCD_IO_Init(void)
{
    GPIO_InitTypeDef    GPIO_InitStructure;
    SPI_InitTypeDef     SPI_InitStructure;
    TIM_TimeBaseInitTypeDef   TimeBase_InitStructure;
    TIM_OCInitTypeDef   TIM_OC_InitStructure;
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2 | RCC_APB1Periph_TIM2, ENABLE);

    LCD_CS_SET();

    GPIO_PinRemapConfig(GPIO_PartialRemap2_TIM2,ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    TimeBase_InitStructure.TIM_ClockDivision = 0;
    TimeBase_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TimeBase_InitStructure.TIM_Period = 10000-1; //1Khz PWM����
    TimeBase_InitStructure.TIM_Prescaler = 14-1;
    TimeBase_InitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TimeBase_InitStructure);

    TIM_OC_InitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OC_InitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OC_InitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC_InitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OC_InitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OC_InitStructure.TIM_OCNPolarity = TIM_OCNPolarity_Low;
    TIM_OC_InitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    /* Keep the backlight off until the boot logo has been drawn. */
    TIM_OC_InitStructure.TIM_Pulse = 0;
    TIM_OC4Init(TIM2,&TIM_OC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);

    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CRCPolynomial = 0;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_16b;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_I2S_DMACmd( SPI2, SPI_I2S_DMAReq_Tx, ENABLE );
    SPI_Cmd(SPI2,ENABLE);
    // NVIC_InitTypeDef NVIC_InitStructure = {0};
    // NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn; // ����ʹ�� EXTI2 ͨ��
    // NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; // �����ȼ�����Ϊ1����ֵԽС���ȼ�Խ�ߡ������ȼ��ߵ��жϻ���ռ���ȼ��͵��ж�
    // NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0; // �����ȼ�����Ϊ2����ֵԽС���ȼ�Խ�ߣ������ȼ�����������ռ
    // NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // ʹ���ж�
    // NVIC_Init(&NVIC_InitStructure);
//    NVIC_EnableIRQ(DMA1_Channel5_IRQn);
}

void LCD_SetLight(uint8_t duty)  //0~100���ȵ���
{
    if (duty > 100U) {
        duty = 100U;
    }
    TIM2->CH4CVR = duty*duty;
}

void LCD_DMA_Transfer(uint16_t* src,uint32_t len)
{
    DMA1_Channel5->CFGR &= ~DMA_CFGR1_EN;//�ر�DMA׼���޸ĳ��Ⱥ͵�ַ
    DMA1_Channel5->CNTR = len;  //�޸Ĵ��䳤��
    DMA1_Channel5->MADDR = (uint32_t)src; //�޸����ݵ�ַ
    DMA1_Channel5->PADDR = (uint32_t)&SPI2->DATAR;
    DMA1_Channel5->CFGR = 0xffff0000 | DMA_Priority_High |  DMA_PeripheralDataSize_HalfWord |
    DMA_MemoryDataSize_HalfWord |  DMA_DIR_PeripheralDST | DMA_IT_TC |
    DMA_MemoryInc_Enable | DMA_CFGR1_EN; //�����µ�DMA���䣬��ַ����
}

void LCD_WR_REG(uint16_t dat)
{
	LCD_DC_CLR();//д����
	LCD_CS_CLR();
    while((SPI2->STATR & SPI_STATR_TXE) == 0);
	SPI2->DATAR = dat;
    while((SPI2->STATR & SPI_STATR_BSY));
	LCD_CS_SET();
	LCD_DC_SET();//д����
}

void LCD_WR_DATA(uint16_t dat)
{
	LCD_CS_CLR();
	while((SPI2->STATR & SPI_STATR_TXE) == 0);
	SPI2->DATAR = dat;
    while((SPI2->STATR & SPI_STATR_BSY));
	LCD_CS_SET();
}

void LCD_WR_DATA_FAST(uint16_t dat)
{
    while((SPI2->STATR & SPI_STATR_TXE) == 0);
	SPI2->DATAR = dat;
}

void LCD_SetWindows(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
{
	LCD_WR_REG(0x2a);//�е�ַ����
	LCD_WR_DATA(x1);
	LCD_WR_DATA(x2);
	LCD_WR_REG(0x2b);//�е�ַ����
	LCD_WR_DATA(y1);
	LCD_WR_DATA(y2);
	LCD_WR_REG(0x2c);//������д
}

void LCD_ResetWindows(void)
{
//    SPI_Cmd( SPI2, DISABLE );
//    SPI2->CTLR1 &= ~SPI_DataSize_16b;
//    SPI_Cmd( SPI2, ENABLE );
    LCD_CS_SET();
    LCD_SetWindows(0,0,240-1,240-1);
    LCD_CS_CLR();
//    SPI_Cmd( SPI2, DISABLE );
//    SPI2->CTLR1 |= SPI_DataSize_16b;
//    SPI_Cmd( SPI2, ENABLE );
}

void LCD_Clear(uint16_t color)
{
    LCD_ResetWindows();
	LCD_CS_CLR();
	for(uint16_t i=0;i<240*240;i++)
	{
		LCD_WR_DATA_FAST(color);
	}
	while((SPI2->STATR & SPI_STATR_BSY));
	LCD_CS_SET();
}

void LCD_Clear_DMA(uint16_t color)
{
    static uint16_t src = 0;
    LCD_CS_SET();
    src = color;
    LCD_ResetWindows();
	LCD_CS_CLR();
    //while(DMA1_Channel5->CNTR!=0); //�ȴ�DMA�������
	DMA1_Channel5->CFGR &= ~DMA_CFGR1_EN;//�ر�DMA׼���޸ĳ��Ⱥ͵�ַ
    DMA1_Channel5->CNTR = 240*240;  //�޸Ĵ��䳤��
    DMA1_Channel5->MADDR = (uint32_t)&src; //�޸����ݵ�ַ
    DMA1_Channel5->PADDR = (uint32_t)&SPI2->DATAR;
    DMA1_Channel5->CFGR = 0xffff0000 | DMA_Priority_High |  DMA_PeripheralDataSize_HalfWord |
    DMA_MemoryDataSize_HalfWord |  DMA_DIR_PeripheralDST | DMA_IT_TC |
    DMA_MemoryInc_Disable | DMA_CFGR1_EN; //�����µ�DMA���䣬��ַ�̶�
	while((SPI2->STATR & SPI_STATR_BSY));
	LCD_CS_SET();
}

void LCD_Draw(uint16_t *data,uint16_t len)
{
	LCD_CS_CLR();
	for(uint16_t i=0;i<len;i++)
	{
		LCD_WR_DATA_FAST(*data);
		data++;
	}
	while((SPI2->STATR & SPI_STATR_BSY));
	LCD_CS_SET();
}

void LCD_Init(void)
{
    #define USE_HORIZONTAL 0
    LCD_IO_Init();
    LCD_RST_SET();
	Delay_Ms(10);	
	LCD_RST_CLR();//��λ
	Delay_Ms(10);
	LCD_RST_SET();
	Delay_Ms(120);
	LCD_WR_REG(0x11);     
	Delay_Ms(120);    
	LCD_WR_REG(0x36);     
	if(USE_HORIZONTAL==0)LCD_WR_DATA(0x00);
	else if(USE_HORIZONTAL==1)LCD_WR_DATA(0xC000);
	else if(USE_HORIZONTAL==2)LCD_WR_DATA(0x7000);
	else LCD_WR_DATA(0xA000);

	LCD_WR_REG(0x3A);     
	LCD_WR_DATA(0x0500); 

	LCD_WR_REG(0x3A);     
	LCD_WR_DATA(0x0500); 

    LCD_WR_REG(0xB2);     
	LCD_WR_DATA(0x1F1F); 
    LCD_WR_DATA(0x0033); 
    LCD_WR_DATA(0x3300); 

	LCD_WR_REG(0xB7);
	LCD_WR_DATA(0x0000); 	

    LCD_WR_REG(0xBB);
	LCD_WR_DATA(0x3600); 	

    LCD_WR_REG(0xC2);
	LCD_WR_DATA(0x0100); 	

    LCD_WR_REG(0xC3);
	LCD_WR_DATA(0x1300);

    LCD_WR_REG(0xC4);
	LCD_WR_DATA(0x2000);

    LCD_WR_REG(0xC6);
	LCD_WR_DATA(0x1300);

    LCD_WR_REG(0xD6);
	LCD_WR_DATA(0xA100);


    LCD_WR_REG(0xD0);
	LCD_WR_DATA(0xA4A0);

     

	LCD_WR_REG(0xE0);   
	LCD_WR_DATA(0xF008);
	LCD_WR_DATA(0x0E09);
	LCD_WR_DATA(0x0804);
	LCD_WR_DATA(0x2F33);
	LCD_WR_DATA(0x4536);
	LCD_WR_DATA(0x1312);
	LCD_WR_DATA(0x2A2D);
	
	LCD_WR_REG(0xE1);   
	LCD_WR_DATA(0xF00E);
	LCD_WR_DATA(0x120C);
	LCD_WR_DATA(0x0A15);
	LCD_WR_DATA(0x2E32);
	LCD_WR_DATA(0x4439);
	LCD_WR_DATA(0x1718);
	LCD_WR_DATA(0x2B2F);

    LCD_WR_REG(0xE4);  
    LCD_WR_DATA(0x1D00);
    LCD_WR_DATA(0x0000);
    
	LCD_WR_REG(0x2129);  
    #define RED     0xF800
    #define GREEN   0x07E0
    #define BLUE    0x001F
    LCD_Clear(BLUE);
    LCD_ResetWindows();
}

// void LCD_Init(void)
// {
//     #define USE_HORIZONTAL 0
//     LCD_IO_Init();
//     LCD_RST_SET();
// 	Delay_Ms(10);	
// 	LCD_RST_CLR();//��λ
// 	Delay_Ms(10);
// 	LCD_RST_SET();
// 	Delay_Ms(120);
// 	LCD_WR_REG(0x11);     
// 	Delay_Ms(120);    
// 	LCD_WR_REG(0x36);     
// 	if(USE_HORIZONTAL==0)LCD_WR_DATA(0x00);
// 	else if(USE_HORIZONTAL==1)LCD_WR_DATA(0xC000);
// 	else if(USE_HORIZONTAL==2)LCD_WR_DATA(0x7000);
// 	else LCD_WR_DATA(0xA000);
// //	LCD_WR_REG(0xB2);
// //	LCD_WR_DATA(0x0101);
// //	LCD_WR_REG(0xC6);     
// //	LCD_WR_DATA(0x0100);   //39Hz
// 	LCD_WR_REG(0xB7);
// 	LCD_WR_DATA(0x0000); 	
// 	LCD_WR_REG(0x3A);     
// 	LCD_WR_DATA(0x5500);  
// 	LCD_WR_REG(0x2129);       

// 	LCD_WR_REG(0xE0);   
// 	LCD_WR_DATA(0xD004);
// 	LCD_WR_DATA(0x080A);
// 	LCD_WR_DATA(0x0905);
// 	LCD_WR_DATA(0x2D43);
// 	LCD_WR_DATA(0x4909);
// 	LCD_WR_DATA(0x1615);
// 	LCD_WR_DATA(0x262B);
	
// 	LCD_WR_REG(0xE1);   
// 	LCD_WR_DATA(0xD003);
// 	LCD_WR_DATA(0x090A);
// 	LCD_WR_DATA(0x0A06);
// 	LCD_WR_DATA(0x2E44);
// 	LCD_WR_DATA(0x403A);
// 	LCD_WR_DATA(0x1515);
// 	LCD_WR_DATA(0x262A);
//     #define RED     0xF800
//     #define GREEN   0x07E0
//     #define BLUE    0x001F
//     LCD_Clear(BLUE);
//     LCD_ResetWindows();  
// }



//void Display_usb(uint16_t *data,uint32_t len)
//{
//    if(*data==1)
//    {
//        SPI_Cmd( SPI1, DISABLE );
//        SPI1->CTLR1 &= ~SPI_DataSize_16b;
//        SPI_Cmd( SPI1, ENABLE );
////        LCD_SetWindows(0,0,240-1,320-1);
//        SPI_Cmd( SPI1, DISABLE );
//        SPI1->CTLR1 |= SPI_DataSize_16b;
//        SPI_Cmd( SPI1, ENABLE );
//        LCD_CS_CLR;
//        LCD_RS_SET;
//    }
//    data+=2;
//    len-=4;
//    if(len)
//        LCD_DMA_Transfer(data,len/2);
////   LCD_CS_SET;
//    //for(uint32_t i=0;i<len;i++)
//     //   Lcd_WriteData_16Bit(*data++);
//}


