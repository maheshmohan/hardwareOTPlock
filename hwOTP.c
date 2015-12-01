/*
 * File:   hwOTP.c
 * Authors: Jishnu Shankar  (CB.EN.P2EBS15013)
 *          Mahesh M        (CB.EN.P2EBS15017)
 *          Vivek Ashokan   (CB.EN.P2EBS15026)
 *          
 *Description: This is the final code for hardware OTP lock. The unlock button will 
 *             be connected to pin RB0 pin of PIC16F877A which will generate an
 *             interrupt for any signal change in the pin. The interrupt service routine 
 *             will then generate a random number and transmits it via UART to 
 *             the GSM module. System then waits for the user to input the OTP and sends 
 *             an unlock signal on getting a matching OTP.
 * 
 * Random Number : 4 digit random number is generated using rand() function. The seed 
 *                 value is the timer 2 value taken at fixed intervals/ at the time of
 *                 generation of an OTP. To increase the randomness, seed is updated 
 *                 on each ISR call.
 *  
 */


#include <xc.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>

#define _XTAL_FREQ 20000000             //20MHz

//LCD pins
#define RS RC3                          //RS pin for LCD
#define RW RC4                          //Read/Write enabler for LCD
#define EN RC5                          //LCD Enable
#define LCD_PORT PORTD					//LCD Data
#define RS_T TRISC3					    //LCD Data/Command mode elect
#define RW_T TRISC4					    //LCD Read/Write Enable
#define EN_T TRISC5					    //LCD Enable

#define BAUD_RATE 9600                  //Baud rate = 9600Hz  
#define BAUD_VAL 129  



//define indicator LED pins
#define RED RC0							//Locked
#define YELLOW RC1						//Waiting for OTP input
#define GREEN RC2						//Unlocked

//store seed value
int seed; 
  
//Global Variables for holding random number
char rn_send[4]="0000";
char otp_receive[4];
char KeyArray[4][3]={'1','2','3','4', '5','6','7','8','9','*','0','#'};
int rn,col,row,pos;


//declare functions
void init_timer2();                 //timer2 initialize
void init_PORT();                   //initialize ports
void init_intcon();                 //initialize interrupts
void init_UART();                   //initialize UART
void byteWrite(char);               //send a character via UART
void wordWrite(char *);             //send a string via UART

char getDispValue(int i);           //convert int number to 
									//corresponding ASCII character array
void GSM(char *);                   //send OTP via GSM
void scanrow(void);                 //scan keypad row
void scancol(void);                 //scan column
void keypad_read(void);             //read keypad
int string_comp(char [],char[]);    //string compare
void interrupt OTP_ISR();           //Interrupt Service Routine

//LCD Fns
void lcd_bytewrt(unsigned char value);
void lcd_command(unsigned char com);
//void lcd_initial(void);
void lcd_wordwrt(unsigned char * text);
void lcd_bulkrst();
void lcd_change(unsigned char * ,unsigned char * );

//define LCD messages 
unsigned char lock[] = " System Locked   ";
unsigned char enter_key[] = " Enter OTP   ";
unsigned char success[] = " System Unlocked   ";
unsigned char fail[] = " Invalid OTP   ";
unsigned char welcome[] = "Press Unlock Key      ";
unsigned char warning[] = "Auto lock in 5s ";

void main(void) {

    //initialize and turn on timer 2
    init_timer2();
    
    //configure ports
    init_PORT();
    
    //initialize UART
    init_UART();
    
    //initialize interrupts
    init_intcon();
    
    //Initialize LCD
    //lcd_initial();
    lcd_change(&lock,&welcome);
    
    //seed random number generator
    seed = TMR2;
    srand(seed);
    
    
    //LCD_com_dat(0x80,lock);
    //LCD_com_dat(0xC0,enter_key);
    //stringLCD(&lock);
    
    while(1);
    
}

//initialize timer2 to generate seed for random function
void init_timer2(){
    //prescale = 1:4
    T2CON |= 0x01;
    //initialize timer2 with value from eeprom seed value
    TMR2 = seed;
    //turn on timer 2
    T2CON |= 0x04;
}

//initialize ports
void init_PORT(){
    TRISB &= 0x00;
    TRISB |= 0x01;
    nRBPU = 0;
    TRISC &= 0x00;
    TRISE &= 0x00;
    TRISD &= 0x00;
    //PORTD &= 0x00;
    //LCD changes
    PORTD=0xFF;
    
    //LCD changes end
    GREEN = 0;
    
    YELLOW = 0;
    RED = 1;
}

//initialize interrupts
void init_intcon(){
    INTE = 1;
    GIE = 1;
    //INTEDG = 0;
    OPTION_REG &= 0x3F;
    INTF = 0;
}

//funtion to initiaize UART
void init_UART(){
    PORTC |= 0x80;
    TXSTA |= 0x26;
    RCSTA |= 0x80;
    SPBRG = BAUD_VAL;
}

//write a byte
void byteWrite(char data){
    while(!TRMT);
    TXREG = data;
}

//write string
void wordWrite(char *data){
    while(*data){
        byteWrite(*data);
        data++;
    }
}

//convert digits to corresponding ASCII charachets
char getDispValue(int i){
    switch(i){
        case 0 : return 0x30; break;
        case 1 : return 0x31; break;
        case 2 : return 0x32; break;
        case 3 : return 0x33; break;
        case 4 : return 0x34; break;
        case 5 : return 0x35; break;
        case 6 : return 0x36; break;
        case 7 : return 0x37; break;
        case 8 : return 0x38; break;
        case 9 : return 0x39; break;
        default: return 0x30;
    }
}

//function to send OTP via GSm
void GSM(char *pwd)
{ 
    int i,j;

    // Check GSM Module
    wordWrite("AT\r\n");
    __delay_ms(1000);                        // Delay for short duration
  
    // Enable text format in GSM Module
    wordWrite("AT+CMGF=1\r\n");
    __delay_ms(1000); 
  
    //Send text message via GSM  
    wordWrite("AT+CMGS=\"9496359883\"\r\n");
    __delay_ms(1000); 
 
    char name[15] = "The OTP is ";
    for (i=11,j=0;i<=14;i++,j++){
        name[i] = *(pwd+j);
    }
  
    wordWrite(name);
    __delay_ms(1000);
  
    wordWrite("\r\n");         //Send enter
    __delay_ms(1000);
  
    byteWrite(26);             //Send Ctrl+z ascii value
    //__delay_ms(1000);
  
  
    return; 
}

//scan row
void scanrow(){        
 	switch(PORTB){      
		case 0x71:          row=3;            
		break;       
		case 0xB1:          row=2;             
		break;       
		case 0xD1:          row=1;           
		break;      
		case 0xE1:          row=0;             
		break;    
		}
}

//scan column
void scancol(){         
	switch(PORTB) {       
		//case 0x0e:          col=3;
		//break;
		case 0x0D:          col=2;         
		break;      
		case 0x0B:          col=1;        
		break;      
		case 0x07:          col=0;         
		break;   
		}
}

//read keypad
void keypad_read(void){
        TRISB=0xf1;      
        PORTB=0xf1;      
        while(PORTB==0xf1);      
        scanrow();  
        __delay_ms(1);

        TRISB=0x0f;     
        PORTB=0x0f;      
        while(PORTB==0x0f);         
        scancol();        
        __delay_ms(2);
        
        TRISB=0x0f;     
        PORTB=0x0f;    
        
        otp_receive[pos] = KeyArray[row][col];
}

//string compare
int string_comp(char a[],char b[]){
    int match_count = 0;
    for(int count=0;count<4;count++){
        if(a[count]==b[count]){
            match_count++;
        }
    }
    return(match_count);
}

//Interrupt Service Routine
void interrupt OTP_ISR(){
    short int count;
    //external interrupt
    if(INTF){
        row=0;col=0;pos=0;
        
        //change seed
        seed = TMR2;
        srand(seed);
        
        //Generate random number
        rn = (rand()%9999)+1;
        
        //convert the number to string
        for(count=0;count<4;count++){
            rn_send[count]=getDispValue(rn%10);
            rn/=10;
        }
        //send generated random number via UART
        GSM(&rn_send);
        
        //Indicate keypad ready to accept password
        YELLOW = 1;
        //lcd_change(&lock,&enter_key);
        lcd_command(0x01);
        lcd_command(0x80);
        lcd_wordwrt(&enter_key);
        __delay_ms(1);
        lcd_command(0xC0);
        
        //read keypad
        for(pos=0;pos<4;pos++){
            keypad_read();
            __delay_ms(250);
            lcd_bytewrt(otp_receive[pos]);
        }
       
        //debug
        //wordWrite(&otp_receive);
        //wordWrite(&rn_send);
        
        if(string_comp(rn_send,otp_receive)==4){
            //debug
            //wordWrite("Success");
            lcd_change(&success,&warning);
            GREEN = 1;
            RED = 0;
            __delay_ms(5000);
            lcd_change(&lock,&welcome);
            GREEN = 0;
            RED = 1;
        }
        else{
            GREEN = 0;
            RED = 1;
            lcd_change(&fail,&lock);
            __delay_ms(3000);
            lcd_change(&lock,&welcome);
        }
        YELLOW = 0;
        
        //clear INTF
        INTF = 0;
    }
}

void lcd_command(unsigned char xcom)
{ 
RS=0;
RW=0;
EN=1;
//PORTD = xcom;
LCD_PORT=xcom;
__delay_ms(5);
EN=0;
}

void lcd_bytewrt(unsigned char value1)
{ 
RS=1;
RW=0;
EN=1;
//PORTD=value1;
LCD_PORT=value1;
__delay_ms(5);
EN=0;
}
//
//void lcd_initial(void)
//{
//RS_T = 0;
//RW_T = 0;
//EN_T = 0;
//
//
//	
//    lcd_command(0x38);
//    lcd_command(0x0C);
//    lcd_command(0xC0);
//    lcd_command(0x38);
//    lcd_command(0x38);
//    
//    lcd_wordwrt("Status : ");
////    lcd_data('T');
////    lcd_data('e');
////    lcd_data('m');
////    lcd_data('p');
////    lcd_data(':');
//    
//    lcd_command(0xC9);
//    lcd_wordwrt(&lock);
////    lcd_data(' ');        
////    lcd_data('C');   
//    
//      lcd_command(0X80); 
//      lcd_wordwrt(" H/W OTP LOCK ");
//}


void lcd_change(unsigned char * status,unsigned char * message){
    RS_T = 0;
    RW_T = 0;
    EN_T = 0;
    LCD_PORT=0xFF;
	
    lcd_command(0x38);
    lcd_command(0x0C);
    lcd_command(0xC0);
    lcd_command(0x38);
    lcd_command(0x38);
    
//    lcd_wordwrt("Status: ");
//    lcd_data('T');
//    lcd_data('e');
//    lcd_data('m');
//    lcd_data('p');
//    lcd_data(':');
    
    //lcd_command(0xC9);
    lcd_wordwrt(status);
//    lcd_data(' ');        
//    lcd_data('C');   
    
      lcd_command(0X80); 
      lcd_wordwrt(message);
}


void lcd_wordwrt(unsigned char *text){
    int i;

  for(i=0;text[i]!='\0';i++)
     lcd_bytewrt(text[i]); 
    
}
