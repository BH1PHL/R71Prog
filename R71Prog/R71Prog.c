#include <stdlib.h>
#include <stdint.h> 
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <string.h>

#include "bitmanip.h"
#include "uart.h"
#include "printf.h"

#define UART_BAUD_RATE 57600

#define CD4040_CLK_L() CLB(PORTC, BIT(4))
#define CD4040_CLK_H() STB(PORTC, BIT(4))
#define CD4040_RST_L() CLB(PORTC, BIT(5))
#define CD4040_RST_H() STB(PORTC, BIT(5))

#define WR_L() CLB(PORTB, BIT(0))
#define WR_H() STB(PORTB, BIT(0))
#define WP_L() CLB(PORTD, BIT(7))
#define WP_H() STB(PORTD, BIT(7))
#define CE_L() CLB(PORTD, BIT(2))
#define CE_H() STB(PORTD, BIT(2))

#define RD_NIBBLE() (PINC & 0x0f)
#define WR_NIBBLE(n) do{CLB(PORTC, BIT(0)|BIT(1)|BIT(2)|BIT(3)); STB(PORTC, (n)&0x0f);}while(0)

#define RD_SET()	do{CLB(DDRC, BIT(0)|BIT(1)|BIT(2)|BIT(3)); WR_NIBBLE(0x0f);}while(0)
#define WR_SET()	STB(DDRC, BIT(0)|BIT(1)|BIT(2)|BIT(3))

#define LED_ON()	CLB(PORTB, BIT(1))
#define LED_OFF()	STB(PORTB, BIT(1))

const char Prompt_S[] PROGMEM = "Read, Program, Verify, List? ";
const char N2CBUFormat_S[] PROGMEM = "Paste N2CBU format IC-R71 RAM data, end with a blank line:\r\n";
const char Offset_S[] PROGMEM = " Offset: ";
const char Modes_S[][5] PROGMEM ={"LSB ","USB ","AM  ", "CW  ", "RTTY", "FM  "};
const char CrLf[] PROGMEM = "\r\n";


#define LINELENGTH 81
char Linebuf[LINELENGTH];
uint8_t Linepos;
char Strbuf[15];

int8_t Offset[5];

void getline_uart(void);
void read_R71_mem(void);
uint8_t read_nibble_adr_inc(void);

void print_N2CBU_file_head(void);
void list_R71_mem(void);
void print_offset(uint16_t* nextadrp, int8_t* offsetp);
void print_mem(uint16_t* nextadrp);
void print_freqlimit(uint16_t* nextadrp);
void readfreq(char* buf, uint16_t* nextadrp, int8_t offset);

void prog_R71_mem(void);
void putch(void* p, char c);
void write_nibble_adr_inc(uint8_t nibble);
uint8_t hex_char_to_int(char c);
void verify_R71_mem(void);

int main(void){

	cli();
	CE_H();
	STB(DDRD, BIT(2));

	CD4040_CLK_L();
	CD4040_RST_H();
	STB(DDRC, BIT(4)|BIT(5));
	RD_SET();
	_delay_ms(10);
	WR_H();
	STB(DDRB, BIT(0));
	WP_L();
	STB(DDRD, BIT(7));

	LED_OFF();
	STB(DDRB, BIT(1));

	uart_init(UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU)); 
	init_printf(uart_putc);
	sei();
	
	//uart_puts_P("> ");
	uart_puts_P("\r\nIC-R71 Programmer by BH1PHL\r\n");
	uart_puts_P("Please connect with 57600 8-N-1 Xon/Xoff flow control.\r\n");
	uart_puts_p(Prompt_S);
	while(1){
		
		getline_uart();
		//Process line

		//uart_puts(Linebuf);
		//uart_puts_P("\r\n");

		switch(Linebuf[0]){
		case 'r':
		case 'R':
			uart_puts_P("Read\r\n");
			read_R71_mem();
			break;

		case 'p':
		case 'P':
			uart_puts_P("Program\r\n");
			prog_R71_mem();
			break;
		case 'v':
		case 'V':
			uart_puts_P("Verify\r\n");
			verify_R71_mem();
			break;
		case 'l':
		case 'L':
			uart_puts_P("List\r\n");
			list_R71_mem();
			break;
		default:
			break;
		}
		uart_puts_p(Prompt_S);
	}
}


void print_N2CBU_file_head(void){
	uint8_t i, j;

	for (i=0;i<6;i++){
		uart_putc(' ');
	}
	for (j=1;j<=3;j++){
		for (i=0;i<14;i++){
			uart_putc(' ');
		}
		uart_putc('+');
		uart_putc(j+'0');
	}

	uart_puts_p(CrLf);

	for (i=0;i<5;i++){
		uart_putc(' ');
	}
	for (j=1;j<=4;j++){
		for (i=0;i<16;i++){
			printf("%1X", i);
		}
	}
	uart_puts_p(CrLf);

	for (i=0;i<5;i++){
		uart_putc(' ');
	}

	for (i=0;i<64;i++){
		uart_putc('|');
	}

}

void read_R71_mem(void){
	uint16_t i;
	print_N2CBU_file_head();
	LED_ON();
	RD_SET();
	WR_H();
	CD4040_RST_L();

	for(i=0;i<0x400;i++){
		if (i%0x40==0){
			uart_puts_p(CrLf);
			printf("%03X: ",i);		
		}

		printf("%01X", read_nibble_adr_inc());

	}

	CD4040_RST_H();
	uart_puts_p(CrLf);
	LED_OFF();
}

void list_R71_mem(void){
	uint16_t nextadr;
	uint8_t i;
	nextadr=0;
	LED_ON();
	RD_SET();
	WR_H();
	CD4040_RST_L();

	while(1){
		read_nibble_adr_inc();
		nextadr++;

		switch(nextadr){
		case 0x10:	//print offset
			for(i=0;i<5;i++){
				uart_puts_p(Modes_S[i]);
				print_offset(&nextadr, &Offset[i]);
				uart_puts_P("   ");
				if (i==2) uart_puts_p(CrLf);
			}
			uart_puts_p(CrLf);
			uart_puts_p(CrLf);
			break;

		case 0x2d:
			print_freqlimit(&nextadr);
			uart_puts_p(CrLf);
			break;

		case 0x12a:	//print VFOa VFOb MEM1..32
			print_mem(&nextadr);
			break;

		default:
			break;
		}

		if (nextadr>=0x400) break;	
	}

	CD4040_RST_H();
	LED_OFF();	
}

//Read and print 9-nibble little-endian format frequency data.
//Frequency data format:
//ff.fff ff00<-Most significant nibble
//   k   M
void readfreq(char* buf, uint16_t* nextadrp, int8_t offset){
	uint8_t i;
	int8_t freqM; //, freq10Hz;
	int16_t freq100Hz;

	for(i=0;i<9;i++){
		buf[i]=read_nibble_adr_inc();
		(*nextadrp)++;
	}
	if (buf[0]==0xf){
		uart_puts_P("BLANK");
		for(i=0;i<13;i++){
			uart_putc(' ');
		}

		return;
	}
	freqM=buf[6]*10+buf[5];

	freq100Hz=buf[4]*1000+buf[3]*100+buf[2]*10+buf[1];

//	freq10Hz=buf[1]*10+buf[0];

	freq100Hz+=offset;
	if (freq100Hz>=10000){
		freq100Hz-=10000;
		freqM+=1;
	}else if (freq100Hz<0){
		freq100Hz+=10000;
		freqM-=1;
	}

	printf("%2d %03d.%02d", freqM, freq100Hz/10, (freq100Hz%10)*10+buf[0]);
/*
	for(i=6;i<=6;i--){
		uart_putc(buf[i]+'0');
		if (i==5) uart_putc(' ');
		if (i==2) uart_putc('.');
	}
*/

	uart_puts_P(" kHz ");

}

void print_freqlimit(uint16_t* nextadrp){

	uart_puts_P("LLimit: ");
	readfreq(Strbuf, nextadrp, 0);
	uart_puts_P("      ");


	uart_puts_P("HLimit: ");
	readfreq(Strbuf, nextadrp, 0);
	uart_puts_p(CrLf);
}

void print_mem(uint16_t* nextadrp){
	uint8_t i, mode;
	int8_t memslot;	//-1: VFOA; 0: VFOB, 1..32: MEM1..32

	for(memslot=-1;memslot<=32;memslot++){
		if (memslot==-1){
			uart_puts_P("VFO  A: ");
		}else if (memslot==0){
			uart_puts_P("VFO  B: ");
		}else{
			uart_puts_P("MEM ");
			printf("%02d: ", memslot);

		}
		//read dummy data
		for(i=0;i<8;i++){
			read_nibble_adr_inc();
			(*nextadrp)++;
		}
		//read mode
		mode=read_nibble_adr_inc();
		(*nextadrp)++;
		//read and print freq
		readfreq(Strbuf, nextadrp, Offset[mode]);
		//print mode
		if (mode<=5) uart_puts_p(Modes_S[mode]);
		
		if (memslot%2==0){
			uart_puts_p(CrLf);
		}else{
			uart_puts_P("  ");
		}
	}
}

void print_offset(uint16_t* nextadrp, int8_t* offsetp){
	uint8_t o1, o2;
	
	uart_puts_p(Offset_S);
	o1=read_nibble_adr_inc();
	o2=read_nibble_adr_inc();
	(*nextadrp)+=2;
	(*offsetp)=((int16_t)(o2&0b111))*10+o1;
	if (o2>=8) (*offsetp)=-(*offsetp);
	if ((*offsetp)<0){
		uart_putc('-');
	}else{
		uart_putc(' ');
	}

	printf("%1d.%1d kHz", o2&0b111, o1);

}

uint8_t read_nibble_adr_inc(void){
		uint8_t nibble;
		//read nibble
		CE_L();
		_delay_us(10);
		nibble=RD_NIBBLE();
		CE_H();

		//toggle CD4040
		CD4040_CLK_H();
		_delay_us(5);
		CD4040_CLK_L();
		_delay_us(5);

		return nibble;
}



void prog_R71_mem(void){
	uint16_t i, adr;
	//uint8_t nibble;
	adr=0;
	LED_ON();
	WR_SET();
	WR_H();
	WP_H();
	CD4040_RST_L();
	uart_puts_p(N2CBUFormat_S);

	while(1){

		getline_uart();
		if (strlen(Linebuf)==0) break;
		if ((strlen(Linebuf)>=69) && Linebuf[0]>='0' && Linebuf[0]<='3'){
			for (i=5;i<69;i++){
				if (adr<=0x3ff) write_nibble_adr_inc(hex_char_to_int(Linebuf[i]));
				adr++;
			}
		}
		
	}
	CD4040_RST_H();
	WP_L();
	RD_SET();
	LED_OFF();
	
}

uint8_t hex_char_to_int(char c){
	if (c>='0' && c<='9') return c-'0';
	if (c>='A' && c<='F') return c-'A'+10;
	return 255;
}

void write_nibble_adr_inc(uint8_t nibble){
		//set write port
		WR_NIBBLE(0Xf & nibble);

		//write nibble
		CE_L();
		WR_L();
		_delay_us(5);
		WR_H();
		_delay_us(5);
		CE_H();

		//toggle CD4040
		CD4040_CLK_H();
		_delay_us(5);
		CD4040_CLK_L();
		_delay_us(5);	
}

void verify_R71_mem(void){
	uint16_t i;
	uint16_t adr, erradr;
	uint8_t nibble;

	adr=0;
	erradr=65535;
	LED_ON();
	RD_SET();
	WR_H();
	CD4040_RST_L();

	uart_puts_p(N2CBUFormat_S);

	while(1){

		getline_uart();
		if (strlen(Linebuf)==0) break;
		if ((strlen(Linebuf)>=69) && Linebuf[0]>='0' && Linebuf[0]<='3'){
			for (i=5;i<69;i++){
				
				nibble=read_nibble_adr_inc();
				if (hex_char_to_int(Linebuf[i])!=nibble){
					erradr=adr;
				}
				adr++;
			}
		}
		
	}
	CD4040_RST_H();
	LED_OFF();

	if (erradr==65535){
		uart_puts_P("Verify OK.\r\n");
	}else{
		uart_puts_P("Verify ERROR. Last error at ");
		printf("%03X", erradr);
		uart_puts_P(".\r\n");
	}
}


void getline_uart(void){
	uint16_t c;
	Linepos=0;
	uart_putc(0x11);	//Xon
	while(1){
		c = uart_getc();
		if (!(c & UART_NO_DATA)){	
			if ((uint8_t)c==13 || (uint8_t)c==10){
				uart_putc(0x13);	//Xoff
				uart_puts_p(CrLf);
				Linebuf[Linepos]=0;
				break;
			}
			if (((uint8_t)c!=8) && ((uint8_t)c!=127)){
				if (((uint8_t)c>=32) && (Linepos<=LINELENGTH-2)){
					uart_putc((uint8_t)c);	//Echo characters
					Linebuf[Linepos]=(uint8_t)c;
					Linepos++;
				}
				//if (Linepos>=LINELENGTH-1) Linepos=LINELENGTH-1;
			}else{
				if (Linepos>0){		//Process backspace
					uart_putc((uint8_t)c);
					Linepos--;
					//Linebuf[Linepos]=0;
				}
			}
		}
	}	
}

/*
Connections

AD0~AD9:		Q1..Q10 of CD4040
/CE(AD10):		PD2(4) of ATMega48, pull up to +5V with 2.2K
/WR:			PB0(14), pull up to +5V with 2.2K
DB0:			PC0(23)
DB1:			PC1(24)
DB2:			PC2(25)
DB3:			PC3(26)
/WP:			PD7(13)
CLK of CD4040:	PC4(27)
RST of CD4040:	PC5(28)
LED:			PB1(15)
R2O of MAX232:	RXD(2)
T2I of MAX232:	TXD(3)
*/
