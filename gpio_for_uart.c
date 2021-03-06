
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "psam_timer.h"
#include "psam_uart.h"


typedef struct psam_uart_ctrl_t {

	int etu;
	volatile unsigned char send_data;
	volatile unsigned char recv_data;
	
	volatile unsigned char send_flag;
	
	pFunIOSet io_set;
	pFunIOGet io_get;

	pFuncSendByteComplete send_complete;
	pFuncRecvByteComplete recv_complete;
	
}stPsamUartCtrl;

volatile stPsamUartCtrl gPsamUartCtrl;
volatile stPsamUartCtrl *gpPsamUartCtrl = &gPsamUartCtrl;

static stPsamUartCtrl *psamUartGetCtrl( void )
{
	return gpPsamUartCtrl;
}

int get_1_cnt( unsigned char ch )
{
	int cnt = 0;
	while( ch )
	{
		if( ch & 1 )
			cnt++;
		
		ch >>= 1;
	}
	return cnt;
}

static void psamUartSendByteTimerHandler( void )
{
	volatile static int cnt = 0;
	struct psam_uart_ctrl_t *uart_ctrl;
	int status = 1;
	int val = 0;
	
	uart_ctrl = psamUartGetCtrl();
	if( uart_ctrl->io_set == NULL )
	{
		cnt = 1;
		psamTimerStop();
		return ;
	}
	
	cnt++;
	
	// 起始位
	if( cnt == 0 )
	{
		psamTimerStart( uart_ctrl->etu );
		uart_ctrl->io_set( val );
		//printf(val ? "*" : "_");
	}
	else if ( cnt >= 1 && cnt <= 8 ) // 数据位
	{
		psamTimerStart( uart_ctrl->etu );
		#if 0
		if( uart_ctrl->send_data & (1 << (cnt - 2)) )
			uart_ctrl->io_set( 1 );
		else
			uart_ctrl->io_set( 0 );
		#endif
		val = uart_ctrl->send_data & (1 << (cnt - 1));
		uart_ctrl->io_set( val ? 1 : 0 );
		//printf(val ? "*" : "_");
	}
	else if( cnt == 9 )
	{
		psamTimerStart( uart_ctrl->etu );
		// 偶校验，1的位数为偶数
		#if 0
		if( get_1_cnt( uart_ctrl->send_data ) % 2 != 0 )
			uart_ctrl->io_set( 1 );
		else
			uart_ctrl->io_set( 0 );
		#endif
		val = ( get_1_cnt( uart_ctrl->send_data ) % 2 ) != 0 ? 1 : 0;
		uart_ctrl->io_set( val );
		//printf(val ? "*" : "_");
	}
	else if( cnt == 10 )
	{
		//printf("|");
		//cnt = 1;
		//psamTimerStop();
		
		//if( uart_ctrl->send_complete )
		//	uart_ctrl->send_complete( status );

		psamTimerStart( uart_ctrl->etu );
		uart_ctrl->io_set( 1 );
		//psamIoSetMode( 2 );
	}
	else if( cnt == 11 )
	{
		psamTimerStart( uart_ctrl->etu );
		//uart_ctrl->io_set( 1 );
		//psamIoSetMode( 2 );
	}
	else if( cnt == 12 )
	{
		cnt = 0;
		psamTimerStop();
		//psamIoSetMode( 3 );
		
		uart_ctrl->send_flag = 1;
		
		if( uart_ctrl->send_complete )
			uart_ctrl->send_complete( status );
		
	}
}

static void psamUartRecvByteTimerHandler( void )
{
	static int cnt = 0;
	int status = 0;
	int val;
	
	struct psam_uart_ctrl_t *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();
	if( uart_ctrl->io_get == NULL )
	{
		cnt = 0;
		psamTimerStop();
		return ;
	}

	//psamTimerStart( uart_ctrl->etu );

	cnt++;
	
	val = uart_ctrl->io_get();

	
	// 起始位，可以做一些容错判断
	if( cnt == 1 )
	{
		//printf( val ? "+" : "-" );
		psamTimerStart( uart_ctrl->etu );
		
	}
	else if ( cnt >= 2 && cnt <= 9 ) // 数据位
	{
		//printf( val ? "+" : "-" );
		psamTimerStart( uart_ctrl->etu );
		if( val )
			uart_ctrl->recv_data |= ( 1 << ( cnt - 2 ) );
		else
			uart_ctrl->recv_data &= ~( 1 << ( cnt - 2 ) );
	}
	else if( cnt == 10 ) // 校验位
	{
		//printf( val ? "+" : "-" );
		psamTimerStart( uart_ctrl->etu );
		if( ( get_1_cnt( uart_ctrl->recv_data ) + val ) % 2 != 0 ) // 奇偶校验错误
		{
			uart_ctrl->recv_data = 0xFF;
			status = -404;
		}

		if( uart_ctrl->recv_complete )
			uart_ctrl->recv_complete( uart_ctrl->recv_data, status );
	}
	else // 
	{
		//printf("|");
		
		cnt = 0;
		psamTimerStop();
		
		//if( uart_ctrl->recv_complete )
		//	uart_ctrl->recv_complete( uart_ctrl->recv_data, status );
	}
}

void psamUartInit( void )
{
	memset( gpPsamUartCtrl, 0x00, sizeof(stPsamUartCtrl) );
	gpPsamUartCtrl->etu = 372 / 2; // 默认4MHz
}

void psamUartRegistIOSet( pFunIOSet io_set )
{
	stPsamUartCtrl *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();

	uart_ctrl->io_set = io_set;
}

void psamUartRegistIOGet( pFunIOGet io_get )
{
	stPsamUartCtrl *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();

	uart_ctrl->io_get = io_get;
}
void psamUartRegistSendCompleteCallback( pFuncSendByteComplete send_cb )
{
	stPsamUartCtrl *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();

	uart_ctrl->send_complete = send_cb;

}
void psamUartRegistRecvCompleteCallback( pFuncRecvByteComplete recv_cb )
{
	stPsamUartCtrl *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();

	uart_ctrl->recv_complete = recv_cb;

}

int psamUartSetETU( int etu )
{
	stPsamUartCtrl *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();

	uart_ctrl->etu = etu;

	return 0;
}

void psamUartSendWaitComplete( void )
{
	stPsamUartCtrl *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();

	while( uart_ctrl->send_flag == 0 )
		;
}

int psamUartSendStart( unsigned char ch )
{
	stPsamUartCtrl *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();

	uart_ctrl->send_data = ch;
	uart_ctrl->send_flag = 0;
	
	//psamIoSetMode( 0 );
	uart_ctrl->io_set( 0 );
	psamTimerHandlerRegister( psamUartSendByteTimerHandler );

	psamTimerStart( uart_ctrl->etu );

	return 0;
}

int psamUartRecvStart( void )
{
	stPsamUartCtrl *uart_ctrl;

	uart_ctrl = psamUartGetCtrl();
	
	psamTimerHandlerRegister( psamUartRecvByteTimerHandler );
	psamTimerStart( uart_ctrl->etu / 2 );
	
	return 0;
}


