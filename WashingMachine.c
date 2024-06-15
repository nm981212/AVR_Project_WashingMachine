#include <mega128a.h>
#include <stdio.h>
#include <delay.h>
#include <twi.h>
#include <alcd_twi.h>

// ��ũ�� ����
#define SW_wash PIND.4
#define SW_rinse PIND.5
#define SW_dry PIND.6
#define LED_run 0xfe
#define LED_pause 0xfd
#define LED_end 0xfb

// �������� ���� 
bit WashingMachine_ON = 0;
bit Start = 0;
bit pause = 0;
bit PushFlg = 0;
bit WM_flag = 0;
bit modesel_flag = 0;
bit washcnt_flag = 0;
bit Start_flag = 0;
bit selfmode_flag = 0;
bit pause_flag = 1;
unsigned char save_tcntH;
unsigned char save_tcntL;
char alarm[6][20] = { "POWER ON", "START  Washing  ", "START   Rinse   ", "START    Dry    ", "      STOP      ", " End of washing " };
char mode[4][20] = { "STANDARD MODE   ", "POWER MODE     ", "SPEED MODE      ", "SELF MODE       " };
unsigned char wash_count[4][3] = { {2, 1, 1}, {2, 2, 2}, {1, 1, 1}, {0, 0, 0} };   // [standard, power, speed, self][wash, rin, dry]

unsigned char adc_data = 0xff;

// �ð������� ���� ��������
unsigned char msec = 0;
unsigned char sec = 0;
unsigned char min = 0;
unsigned char hour = 0;
unsigned char time_arr[3];   // wash, rinse, dry
unsigned char pause_time[3]; // ��, ��, ��

// External Interrupt 2 service routine
/*
��Ź�� ON / ��� ���� / ���� / �Ͻ����� ����
falling edge
*/
interrupt[EXT_INT2] void ext_int2_isr(void)
{
    #asm("cli")
        if (WashingMachine_ON == 0)  // ��Ź�� ON
        {
            WashingMachine_ON = 1;
            WM_flag = 1;
        }
        else if (WM_flag == 1 && modesel_flag == 1)       // ��� ����
        {
            WM_flag = 0;
        }
        else if (selfmode_flag == 1 && washcnt_flag == 1) // ���� ��� ����
        {
            selfmode_flag = 0;
            Start = 1;
            Start_flag = 1;
        }
        else if (Start == 0)         // ����
        {
            Start = 1;
            Start_flag = 1;
        }
        else if (Start == 1)         // �Ͻ� ����
        {
            if (pause == 0 && pause_flag == 0) // �Ͻ� ����
            {
                pause = 1;
                TIMSK = 0x00;
                TCCR1B = 0x08;
                TCCR2 = 0x4b;
                save_tcntH = TCNT1H;
                save_tcntL = TCNT1L;
                pause_time[0] = hour;
                pause_time[1] = min;
                pause_time[2] = sec;
            }
            else if (pause == 1 && pause_flag == 1)     // �Ͻ� ���� ����
            {
                pause = 0;
                TCCR1B = 0x02;
                TCCR2 = 0x7b;
                TIMSK = 0x04;
                hour = pause_time[0];
                min = pause_time[1];
                sec = pause_time[2];
                TCNT1H = save_tcntH;
                TCNT1L = save_tcntL;// 10msec
            }
        }
    #asm("sei")
}

// Ÿ�̸� 1 ���ͷ�Ʈ ����
interrupt[TIM1_OVF] void timer1_ovf_isr(void)
{
    // Reinitialize Timer1 value
    TCNT1H = 0xB1;
    TCNT1L = 0xE0;
    msec++;

    if (msec == 100)
    {
        msec = 0;
        if (sec == 0)
        {
            sec = 59;
            if (min == 0)
            {
                min = 59;
                if (hour == 0)
                {
                    TCCR1B = 0x08;
                    TIMSK = 0x00;
                }
                else hour--;
            }
            else    min--;
        }
        else sec--;
    }
}

// Ÿ�̸�2 - ���� ����� ���� ���ͷ�Ʈ ���� ��ƾ
interrupt[TIM2_COMP] void timer2_comp_isr(void)
{
    TCNT2 = 0x06;
}

// ADC
interrupt[ADC_INT] void adc_isr(void)
{
    adc_data = ADCL;
    adc_data = ADCH;
}

// Buzzer �Լ�
void buzzer(float hz, int count) {
    int j;
    for (j = 0; j < count; j++) {
        PORTB = 0x10;
        delay_ms(((float)1000 / hz) / 2);

        PORTB = 0x00;
        delay_ms(((float)1000 / hz) / 2);
    }
}



void main(void)
{
    // �������� ����
    char str1[20];
    char str2[20];
    char mode_index;
    char motor[3] = { 175, 100, 225 };  // ��Ź�� �ӵ� : ���� ��Ź(70%), ���(40%), Ż��(90%)
    int i;

    // Port ����
    DDRA = PORTA = 0xff;// LED ���
    DDRB = PORTB = 0xff;// ���� ���
    DDRC = PORTC = 0xff;// LCD ���
    DDRD = 0x00;      // PORTD�� �Է����� ��� (switch)
    PIND = 0x00;
    DDRF = PORTF = 0x00;

    // ADC��ȯ
    ADMUX = 0x20;
    ADCSRA = 0xa7;

    // ���ͷ�Ʈ
    EICRA = 0x20;
    EIMSK = 0x04;
    EIFR = 0x04;

    // Ÿ�̸� 0�� - ����
    OCR0 = 0x4b;
    TCNT0 = 0x0006;

    // Ÿ�̸� 1�� ���� - �ð� Ÿ�̸�
    TCCR1A = 0x00;
    TCCR1B = 0x02;

    // Ÿ�̸� 2�� ���� - ���� �ӵ� ����
    TCCR2 = 0x4b;
    OCR2 = 0x00FA;
    TCNT2 = 0x0006;

    // LCD ����� ���� I2C����
    twi_master_init(100);   // 100khz ��� ����
    lcd_twi_init(0x27, 16); // LCD�ּ�, ��� bit�� ����
    #asm("sei")

        while (1)
        {
            while (WashingMachine_ON)
            {
                if (WM_flag)
                {
                    lcd_gotoxy(0, 0);
                    lcd_puts(alarm[0]);
                    ADCSRA |= 0x40;     // ADC start conversion
                    ADCSRA |= 0x08;     // ADC interrupt enable 
                    delay_ms(300);
                }

                // ��� ����
                while (WM_flag)
                {
                    lcd_gotoxy(0, 1);
                    if (adc_data < 63)
                    {
                        mode_index = 0;   // Standard Mode
                        selfmode_flag = 0;
                    }
                    else if (adc_data < 127)
                    {
                        mode_index = 1;   // Power Mode
                        selfmode_flag = 0;
                    }
                    else if (adc_data < 191)
                    {
                        mode_index = 2;   // Speed Mode
                        selfmode_flag = 0;
                    }
                    else
                    {
                        mode_index = 3;   // Self Mode
                        selfmode_flag = 1;
                    }
                    lcd_puts(mode[mode_index]);
                    modesel_flag = 1;
                }

                if (!WM_flag)
                {
                    lcd_gotoxy(0, 0);
                    lcd_puts("mode select!");
                    delay_ms(1000);
                }

                // ���� ��� ����
                while (selfmode_flag)
                {
                    if (!SW_wash)
                    {                     // ����ġ Ȯ��
                        if (!PushFlg)
                        {
                            PushFlg = 1;
                            wash_count[3][0]++;
                            delay_ms(100);
                        };
                    }
                    else    PushFlg = 0;

                    if (!SW_rinse)
                    {
                        if (!PushFlg)
                        {
                            PushFlg = 1;
                            wash_count[3][1]++;
                            delay_ms(100);
                        };
                    }
                    else    PushFlg = 0;

                    if (!SW_dry)
                    {
                        if (!PushFlg)
                        {
                            PushFlg = 1;
                            wash_count[3][2]++;
                            delay_ms(100);
                        };
                    }
                    else    PushFlg = 0;

                    sprintf(str1, "Count - wash: %d", wash_count[3][0]);
                    sprintf(str2, "rinse:%d, dry:%d", wash_count[3][1], wash_count[3][2]);

                    lcd_gotoxy(0, 0);
                    lcd_puts(str1);
                    lcd_gotoxy(0, 1);
                    lcd_puts(str2);
                    washcnt_flag = 1;
                }

                while (Start)
                {
                    if (Start_flag)
                    {
                        lcd_clear();
                        Start_flag = 0;
                        // ������ ��� - �ùķ��̼� ������ ���� ���� 1�� ����
                        time_arr[0] = wash_count[mode_index][0] * 30;
                        time_arr[1] = wash_count[mode_index][1] * 30;
                        time_arr[2] = wash_count[mode_index][2] * 30;
                    }

                    for (i = 0; i < 3; i++)
                    {
                        // �ð� ����
                        hour = 0;
                        min = time_arr[i] / 60;
                        sec = time_arr[i] % 60;
                        msec = 0;

                        // Ÿ�̸� �ѱ� 
                        TCNT1H = 0xB1;
                        TCNT1L = 0xE0;// 10msec
                        TCCR2 = 0x7b;
                        TIMSK = 0x04;

                        // ��Ź �ܰ迡 ���� ������ �ӵ� ����
                        OCR2 = motor[i];
                        pause_flag = 0;

                        while (hour != 0 || min != 0 || sec != 0)
                        {
                            PORTA = LED_run;        // ���� �� LED - ������
                            while (pause)
                            {
                                lcd_gotoxy(0, 0);
                                lcd_puts(alarm[4]);
                                PORTA = LED_pause;  // �Ͻ����� LED : ����� 
                                pause_flag = 1;
                            }
                            pause_flag = 0;

                            lcd_gotoxy(0, 0);
                            lcd_puts(alarm[i + 1]);
                            sprintf(str2, "T: %02d:%02d:%02d", hour, min, sec);
                            lcd_gotoxy(0, 1);
                            lcd_puts(str2);
                        }
                        PORTA = LED_end;            // �� ��尡 ������ �� ��ȣ - LED : �ʷϻ�
                        TCCR2 = 0x4b;
                        delay_ms(100);
                    }

                    // ���� �˸� - LCD
                    lcd_gotoxy(0, 0);
                    lcd_puts(alarm[5]);
                    delay_ms(3000);
                    lcd_clear();

                    // ����
                    for (i = 0; i < 3; i++)
                    {
                        buzzer(480, 24);
                        buzzer(320, 16);
                        delay_ms(1000);
                    }
                    delay_ms(2000);

                    // flag �ʱ�ȭ
                    Start = 0;
                    WashingMachine_ON = 0;
                    washcnt_flag = 0;
                    modesel_flag = 0;
                    pause_flag = 1;

                    // LED OFF
                    PORTA = 0xff;

                    // ���� ��Ź ī��Ʈ �ʱ�ȭ
                    wash_count[3][0] = 0;
                    wash_count[3][1] = 0;
                    wash_count[3][2] = 0;
                }
            }
        }
}