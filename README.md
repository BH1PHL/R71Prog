# R71Prog

#### Introduction
A simple Icom IC-R71 Programmer.
Compiled with AVR Studio 4.19.

#### Connections
![( From http://www.lenrek.net/experiments/r71a-ram-module/ )](README_md_files/r71a-ram-module_20220220102407.jpg?v=1&type=image&token=V1:g2OpPMK8CIHP0Kh7q8t9hkf0wmT63pBYzI3vLa0pGLw)
```
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
----
Crystal:		7.3728MHz
RS-232 Paras:	57600-8-N-1 XON/XOFF
```

