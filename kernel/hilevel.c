/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"

//max 2 processes
pcb_t pcb[2];
pcb_t* current = NULL;
int process_count = 0;

void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
  }

  current = next;                             // update   executing index   to P_{next}

  PL011_putc( UART0, '[', true );
  PL011_putc( UART0, '0' + prev->pid, true );
  PL011_putc( UART0, '-', true );
  PL011_putc( UART0, '>', true );
  PL011_putc( UART0, '0' + next->pid, true );
  PL011_putc( UART0, ']', true );

  return;
}

void schedule( ctx_t* ctx ) {
  pcb_t* prev = current;
  pcb_t* next = current;

  //get previous process and process with highest score
  int maxScore = 0;
  for(int i=0; i<process_count; i++){

    //if last executed process
    if(current->pid == pcb[i].pid){
      prev = &pcb[i];
    }

    int score = pcb[i].age + pcb[i].priority;
    if(maxScore <= score){
      next = &pcb[i];
      maxScore = score;
    }
  }

  //increase age of all processes not executing
  for(int i=0; i<process_count; i++){
    if(next->pid != pcb[i].pid){
      pcb[i].age += 1;
    }
    else{
      pcb[i].age = 0;

    }
  }
  dispatch( ctx, prev, next);
  return;

}

extern void     main_P3();
extern uint32_t tos_P3;
extern void     main_P4();
extern uint32_t tos_P4;
extern void     main_P5();
extern uint32_t tos_P5;

void hilevel_handler_rst( ctx_t* ctx ) {
  PL011_putc( UART0, '[', true );
  PL011_putc( UART0, 'R', true );
  PL011_putc( UART0, 'E', true );
  PL011_putc( UART0, 'S', true );
  PL011_putc( UART0, 'E', true );
  PL011_putc( UART0, 'T', true );
  PL011_putc( UART0, ']', true );

  //init user processes
  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );     // initialise 0-th PCB = P_3
  pcb[ 0 ].pid      = 3;
  pcb[ 0 ].status   = STATUS_CREATED;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_P3 );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_P3  );
  pcb[ 0 ].priority      = 5;
  pcb[ 0 ].age      = 0;

  process_count+=1;

  memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );     // initialise 1-st PCB = P_4
  pcb[ 1 ].pid      = 4;
  pcb[ 1 ].status   = STATUS_CREATED;
  pcb[ 1 ].ctx.cpsr = 0x50;
  pcb[ 1 ].ctx.pc   = ( uint32_t )(&main_P4);
  pcb[ 1 ].ctx.sp   = ( uint32_t )(&tos_P4);
  pcb[ 1].priority      = 1;
  pcb[ 1 ].age      = 0;
  process_count+=1;


  //start executing P3
  dispatch( ctx, NULL, &pcb[ 0 ] );

  //Enable timer interrupts
  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  int_enable_irq();
  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
    PL011_putc( UART0, '[', true );
    PL011_putc( UART0, 'T', true );
    PL011_putc( UART0, 'I', true );
    PL011_putc( UART0, 'M', true );
    PL011_putc( UART0, 'E', true );
    PL011_putc( UART0, 'R', true );
    PL011_putc( UART0, ']', true );
    schedule(ctx);
    TIMER0->Timer1IntClr = 0x01;
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_svc( ctx_t* ctx, uint32_t id ) {

  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      schedule( ctx );
      break;
    }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;

      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
