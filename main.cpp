#include "mbed.h"

I2C i2c(P0_0,P0_1);         //SDA,SCL oled
SPI spi(P0_9,P0_8,P0_7);    //DAC
DigitalOut cs1(P0_6);       //DAC cs

AnalogIn a1(P0_24);     //IM1
AnalogIn a3(P0_26);     //IM2
DigitalOut io0(P2_0);   //EN1. on=1, off=0
DigitalOut io1(P2_1);   //EN2. on=1, off=0
DigitalIn sw0(P1_21);   //rotary1 a
DigitalIn sw1(P1_22);   //rotary1 b
DigitalIn sw2(P1_23);   //rotary2 a
DigitalIn sw3(P1_24);   //rotary2 b
DigitalIn sw4(P1_25);   //en1 sw
DigitalIn sw5(P1_26);   //en2 sw

//OLED
const int oled_adr = 0x78;   //oled i2c adr 0x7C
void oled_init(int adr);     //lcd init func
void char_disp(int adr, int8_t position, char data);    //char disp func
void val_disp(int adr, int8_t position, int8_t digit,int val);  //val disp func
void cont(int adr,uint8_t);     //contrast set

//Rotary state and display
const uint16_t tick_t_on=2000;    //lcd tick on time
const uint16_t tick_t_off=500;    //lcd tick off time
const uint16_t refresh=800;      //val disp refresh rate
uint16_t tc=0;          //tick counter
uint16_t ref_t=0;        //disp refresh counter
uint8_t r1_state;       //rotary 1 state
uint8_t r2_state;       //rotary 2 state
uint8_t r1_val=0;       //0->idle, 1->incr, 2->decr
uint8_t r2_val=0;       //0->idle, 1->incr, 2->decr
uint8_t cur_pos=0;
uint8_t tick_pos=0;

//vals
int16_t vs1=0,vs2=0;        //vset mV unit
uint8_t en1=0,en2=0;
uint16_t im1,im2;
uint8_t disp_val[4];
int16_t vs1_p=0,vs2_p=0;    //past vset values
uint8_t en1_p=0,en2_p=0;    //past en values
uint8_t im1_ov=0,im2_ov=0;  //IM overflow flag

//IM settings
#define RG 2775                     //ILIM R, IM amp gain 300*9.25
#define ilim 980                    //I limit. mA unit
const uint16_t im_t=300;            //im average rate
uint16_t imc=0;                     //im average counter
const uint16_t im_disp_rate=1000;   //im display rate
uint16_t im_disp_cnt=0;             //im display rate counter
float im1_f=0,im2_f=0;
float im1_f_res,im2_f_res;          //im average result (float)
uint16_t im1_res,im2_res;           //im average result (uint) for display

//DAC
#define res 16              //mV expression. 4096mV/2^10 x4(amp gain)
const uint8_t daca=0b1001;  //control command of DAC A
const uint8_t dacb=0b1010;  //control command of DAC B
uint16_t a_val;
uint16_t b_val;
uint16_t spi_buf;
uint8_t spi_rate;           //spi transfer rate

int main(){
    spi.format(16,0);   //spi mode setting. 2byte(16bit) transfer, mode 0
    cs1=1;  //CS high
    io0=0;  //ch1 off
    io1=0;  //ch2 off

    thread_sleep_for(100);  //wait for lcd power on
    oled_init(oled_adr);
    cont(oled_adr,0xff);
    char_disp(oled_adr,2,'.');
    char_disp(oled_adr,5,'V');
    char_disp(oled_adr,6,' ');
    char_disp(oled_adr,10,'m');
    char_disp(oled_adr,11,'A');
    char_disp(oled_adr,0x20+2,'.');
    char_disp(oled_adr,0x20+5,'V');
    char_disp(oled_adr,0x20+6,' ');
    char_disp(oled_adr,0x20+10,'m');
    char_disp(oled_adr,0x20+11,'A');

    while (true){
        
        //rotary scan
        r1_state=((r1_state<<1)+sw0)&0b0011;
        if((r1_state==2)&&(sw1==1))r1_val=1;      //r1 incr
        else if((r1_state==2)&&(sw1==0))r1_val=2; //r1 decr

        r2_state=((r2_state<<1)+sw2)&0b0011;
        if((r2_state==2)&&(sw3==1))r2_val=1;      //r2 incr
        else if((r2_state==2)&&(sw3==0))r2_val=2; //r2 decr        

        if(r2_val==1){
            if(cur_pos<10) ++cur_pos;
            else cur_pos=0;
        }else if(r2_val==2){
            if(cur_pos>0) --cur_pos;
            else cur_pos=8;
        }

        switch(cur_pos){
            case 0:
                tick_pos=16;break;
            case 1:
                tick_pos=0;
                if(r1_val==1)vs1=vs1+10000;
                else if(r1_val==2) vs1=vs1-10000;break;
            case 2:
                tick_pos=1;
                if(r1_val==1)vs1=vs1+1000;
                else if(r1_val==2) vs1=vs1-1000;break;
            case 3:
                tick_pos=3;
                if(r1_val==1)vs1=vs1+100;
                else if(r1_val==2) vs1=vs1-100;break;
            case 4:
                tick_pos=4;
                if(r1_val==1)vs1=vs1+10;
                else if(r1_val==2) vs1=vs1-10;break;
            case 5:
                tick_pos=0x20+0;
                if(r1_val==1)vs2=vs2+10000;
                else if(r1_val==2) vs2=vs2-10000;break;
            case 6:
                tick_pos=0x20+1;
                if(r1_val==1)vs2=vs2+1000;
                else if(r1_val==2) vs2=vs2-1000;break;
            case 7:
                tick_pos=0x20+3;
                if(r1_val==1)vs2=vs2+100;
                else if(r1_val==2) vs2=vs2-100;break;
            case 8:
                tick_pos=0x20+4;
                if(r1_val==1)vs2=vs2+10;
                else if(r1_val==2) vs2=vs2-10;break;
        }

        r1_val=0;
        r2_val=0;

        //EN SW check
        if(sw4==1){         //sw off
            en1=0;
            io0=0;
        }else if(sw4==0){   //se on
            en1=1;
            io0=1;
        }
        if(sw5==1){         //sw off
            en2=0;
            io1=0;
        }else if(sw5==0){   //se on
            en2=1;
            io1=1;
        }

        //vset overflow check
        if(vs1>=15000)vs1=15000;
        if(vs1<=0)vs1=0;
        if(vs2>=15000)vs2=15000;
        if(vs2<=0)vs2=0;
        
        //vset disp
        ++tc;
        if(tc<tick_t_on){
            ++ref_t;
            if(ref_t==refresh){
                ref_t=0;
                //ch1 disp
                if(vs1_p==vs1){
                    disp_val[1]=(vs1/10)%100;    //10+100 mV
                    disp_val[0]=(vs1/10)/100;    //1+10 V
                    val_disp(oled_adr,0,2,disp_val[0]);
                    val_disp(oled_adr,3,2,disp_val[1]);
                }
                if(en1_p==en1){
                    char_disp(oled_adr,13,'O');
                    if(en1==1){
                        char_disp(oled_adr,14,'N');
                        char_disp(oled_adr,15,' ');
                    }else if(en1==0){
                        char_disp(oled_adr,14,'F');
                        char_disp(oled_adr,15,'F');
                    }
                }
                //ch2 disp
                if(vs2_p==vs2){
                    disp_val[3]=(vs2/10)%100;    //10+100 mV
                    disp_val[2]=(vs2/10)/100;    //1+10 V
                    val_disp(oled_adr,0x20+0,2,disp_val[2]);
                    val_disp(oled_adr,0x20+3,2,disp_val[3]);
                }
                if(en2_p==en2){
                    char_disp(oled_adr,0x20+13,'O');
                    if(en2==1){
                        char_disp(oled_adr,0x20+14,'N');
                        char_disp(oled_adr,0x20+15,' ');
                    }else if(en2==0){
                        char_disp(oled_adr,0x20+14,'F');
                        char_disp(oled_adr,0x20+15,'F');
                    }
                }
            }
        }else if(tc==tick_t_on){
            char_disp(oled_adr,tick_pos,' ');
        }else if(tc==tick_t_on+tick_t_off){
            tc=0;
        }else{
            //nothing
        }

        //store past values
        vs1_p=vs1;
        vs2_p=vs2;
        en1_p=en1;
        en2_p=en2;

        //IM average
        if(imc==im_t){
            imc=0;
            im1_f_res=im1_f/im_t;
            im2_f_res=im2_f/im_t;
            im1_f=0;
            im2_f=0;
        }else{
            ++imc;
            im1_f=im1_f+(a1.read()*3300*500/(RG)*2);    //IM & IM average. (val*3300)*500/(RG)*2
            im2_f=im2_f+(a3.read()*3300*500/(RG)*2);
        }
        
        //IM overflow check & IM disp
        ++im_disp_cnt;
        if(im_disp_cnt==im_disp_rate){
            im_disp_cnt=0;
            im1_res=(uint16_t)im1_f_res;
            im2_res=(uint16_t)im2_f_res;
            if(im1_res>ilim){
                im1_res=ilim;
                if(im1_ov==0){
                    im1_ov=1;
                    char_disp(oled_adr,12,'!');
                }
            }else{
                if(im1_ov==1){
                    im1_ov=0;
                    char_disp(oled_adr,12,' ');
                }
            }
            if(im2_res>ilim){
                im2_res=ilim;
                if(im2_ov==0){
                    im2_ov=1;
                    char_disp(oled_adr,0x20+12,'!');
                }
            }else{
                if(im2_ov==1){
                    im2_ov=0;
                    char_disp(oled_adr,0x20+12,' ');
                }
            }
            //IM disp
            val_disp(oled_adr,7,3,im1_res);
            val_disp(oled_adr,0x20+7,3,im2_res);
        }
        
        //SPI transfer
        ++spi_rate;
        if(spi_rate==10){
            spi_rate=0;
            if(en1==0){         //EN1 OFF
                a_val=0;        
            }else if(en1==1){   //EN1 ON
                a_val=vs1/res;
            }
            spi_buf=(daca<<12)+(a_val<<2);  //ch1 dac a value calc.
            cs1=0;
            spi.write(spi_buf);
            cs1=1;
            if(en2==0){         //EN2 OFF
                b_val=0;        
            }else if(en2==1){   //EN2 ON
                b_val=vs2/res;
            }
            spi_buf=(dacb<<12)+(b_val<<2);  //ch2 dac b value calc.
            cs1=0;
            spi.write(spi_buf);
            cs1=1;
        }
    }
}

//LCD init func
void oled_init(int adr){
    char lcd_data[2];
    lcd_data[0] = 0x0;
    lcd_data[1]=0x01;           //0x01 clear disp
    i2c.write(adr, lcd_data, 2);
    thread_sleep_for(20);
    lcd_data[1]=0x02;           //0x02 return home
    i2c.write(adr, lcd_data, 2);
    thread_sleep_for(20);
    lcd_data[1]=0x0C;           //0x0c disp on
    i2c.write(adr, lcd_data, 2);
    thread_sleep_for(20);
    lcd_data[1]=0x01;           //0x01 clear disp
    i2c.write(adr, lcd_data, 2);
    thread_sleep_for(20);
}

void char_disp(int adr, int8_t position, char data){
    char buf[2];
    buf[0]=0x0;
    buf[1]=0x80+position;   //set cusor position (0x80 means cursor set cmd)
    i2c.write(adr,buf, 2);
    buf[0]=0x40;            //ahr disp cmd
    buf[1]=data;
    i2c.write(adr,buf, 2);
}

//disp val func
void val_disp(int adr, int8_t position, int8_t digit, int val){
    char buf[2];
    char data[4];
    int8_t i;
    buf[0]=0x0;
    buf[1]=0x80+position;   //set cusor position (0x80 means cursor set cmd)
    i2c.write(adr,buf, 2);
    data[3]=0x30+val%10;        //1
    data[2]=0x30+(val/10)%10;   //10
    data[1]=0x30+(val/100)%10;  //100
    data[0]=0x30+(val/1000)%10; //1000
    buf[0]=0x40;
    for(i=0;i<digit;++i){
        buf[1]=data[i+4-digit];
        i2c.write(adr,buf, 2);
    }
}

void cont(int adr,uint8_t val){
    char buf[2];
    buf[0]=0x0;
    buf[1]=0x2a;
    i2c.write(adr,buf,2);
    buf[1]=0x79;    //SD=1
    i2c.write(adr,buf,2);
    buf[1]=0x81;    //contrast set
    i2c.write(adr,buf,2);
    buf[1]=val;    //contrast value
    i2c.write(adr,buf,2);
    buf[1]=0x78;    //SD=0
    i2c.write(adr,buf,2);
    buf[1]=0x28;    //0x2C, 0x28
    i2c.write(adr,buf,2);
}
