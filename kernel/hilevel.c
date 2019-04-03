/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"
// query the disk block count
extern int disk_get_block_num();
// query the disk block length
extern int disk_get_block_len();

// write an n-byte block of data x to   the disk at block address a
extern int disk_wr( uint32_t a, const uint8_t* x, int n );
// read  an n-byte block of data x from the disk at block address a
extern int disk_rd( uint32_t a,       uint8_t* x, int n );
//max 20 including console
pcb_t pcb[20];
uint32_t file_table = 0x00000001;
int file_table_size = 8;
uint32_t start_of_file[8] = {0x00000009, 0x00000009+32, 0x00000009+(32*2), 0x00000009+(32*3), 0x00000009+(32*4), 0x00000009+(32*5), 0x00000009+(32*6), 0x00000009+(32*7)};
pcb_t* current = NULL;
int process_count = 0;
void printContextSwitch(int prev, int next){
    PL011_putc( UART0, '\n', true );
    PL011_putc( UART0, 'S', true );
    PL011_putc( UART0, 'W', true );
    PL011_putc( UART0, 'I', true );
    PL011_putc( UART0, 'T', true );
    PL011_putc( UART0, 'C', true );
    PL011_putc( UART0, 'H', true );
    PL011_putc( UART0, ' ', true );
    PL011_putc( UART0, 'A' + prev, true );
    PL011_putc( UART0, '-', true );
    PL011_putc( UART0, '>', true );
    PL011_putc( UART0, 'A'+ next, true );
    PL011_putc( UART0, '\n', true );
}

void dispatch( ctx_t* ctx, pcb_t* prev, pcb_t* next ) {

  if( NULL != prev ) {
    memcpy( &prev->ctx, ctx, sizeof( ctx_t ) ); // preserve execution context of P_{prev}
  }
  if( NULL != next ) {
    memcpy( ctx, &next->ctx, sizeof( ctx_t ) ); // restore  execution context of P_{next}
  }

  current = next;                             // update   executing index   to P_{next}

    
  //printContextSwitch(prev->pid, next->pid);
    PL011_putc( UART0, '\n', true );


  if(prev->status == STATUS_EXECUTING) prev->status = STATUS_READY;
  next->status = STATUS_EXECUTING;
  return;
}

pcb_t* getNextPCB (){
  for(int i=0; i<20; i++){
    if(pcb[i].status == STATUS_TERMINATED) return &pcb[i];
  }
  return NULL;
}

pcb_t* getPCB (pid_t pid){
  for(int i=0; i<20; i++){
    if(pcb[i].pid == pid) return &pcb[i];
  }
  return NULL;
}

void schedule( ctx_t* ctx ) {
  pcb_t* prev = current;
  pcb_t* next = current;
  //get previous process and process with highest score
  int maxScore = 0;
  for(int i=0; i<20; i++){
    if(pcb[i].status != STATUS_TERMINATED){
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
  }
  //increase age of all processes not executing, reset currents
  for(int i=0; i<process_count; i++){
    if(pcb[i].status != STATUS_TERMINATED){
      if(next->pid != pcb[i].pid){
        pcb[i].age += 1;
      }
      else{
        pcb[i].age = 0;
      }
    }
  }
  dispatch( ctx, prev, next);
  return;
}

extern void     main_console();
extern uint32_t tos_user;

void print_to_console( char* x, int n ) {
  for( int i = 0; i < n; i++ ) {
    PL011_putc( UART1, x[ i ], true );
  }
}
extern void itoa( char* r, int x );


void hilevel_handler_rst( ctx_t* ctx ) {

  PL011_putc( UART0, '[', true );
  PL011_putc( UART0, 'R', true );
  PL011_putc( UART0, 'E', true );
  PL011_putc( UART0, 'S', true );
  PL011_putc( UART0, 'E', true );
  PL011_putc( UART0, 'T', true );

  PL011_putc( UART0, ']', true );



// create console process
  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
  pcb[ 0 ].pid      = 0;
  pcb[ 0 ].status   = STATUS_CREATED;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )(&main_console);
  pcb[ 0 ].ctx.sp   = ( uint32_t )(&tos_user);
  pcb[ 0 ].tos   = ( uint32_t )(&tos_user);
  pcb[ 0].priority      = 2;
  pcb[ 0 ].age      = 0;
  process_count+=1;

  //allocate all other PCBs as empty
  for(int i = 1; i < 20; i ++){
    memset( &pcb[ i ], 0, sizeof( pcb_t ) );
    pcb[ i].pid      = i;
    pcb[ i ].status   = STATUS_TERMINATED;
    pcb[ i ].ctx.cpsr = 0x50;
    pcb[ i ].ctx.sp   = ( uint32_t )(&tos_user) - (i*0x00001000);
    pcb[ i ].tos   = pcb[ i ].ctx.sp;
    pcb[ i].priority      = 1;
    pcb[ i ].age      = 0;
    process_count+=1;
  }

  //start executing console
  dispatch( ctx, NULL, &pcb[ 0 ] );

  //Enable timer interrupts
  TIMER0->Timer1Load  = 0x00040000; // select period = 2^18 ticks ~= 1/4 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  int_enable_irq();
    for(int i = 0; i<8; i++){
           disk_wr(0x00000001+i, "0000000000000000", 16);
    }
   disk_wr(0x00000003, "text.txt\0\0\0\0\0\0\0\0", 16);



//     uint8_t* data;
//     disk_rd(0x00000001, data, 16);
//     print_to_console((char *)data, 16);
//     disk_rd(0x00000002, data, 16);
//     print_to_console((char *)data, 16);
  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  // Step 2: read  the interrupt identifier so we know the source.

  uint32_t id = GICC0->IAR;

  // Step 4: handle the interrupt, then clear (or reset) the source.

  if( id == GIC_SOURCE_TIMER0 ) {
//     PL011_putc( UART0, '[', true );
//     PL011_putc( UART0, 'T', true );
//     PL011_putc( UART0, 'I', true );
//     PL011_putc( UART0, 'M', true );
//     PL011_putc( UART0, 'E', true );
//     PL011_putc( UART0, 'R', true );
//     PL011_putc( UART0, ']', true );
    schedule(ctx);
    TIMER0->Timer1IntClr = 0x01;
  }

  // Step 5: write the interrupt identifier to signal we're done.

  GICC0->EOIR = id;

  return;
}


void hilevel_handler_svc( ctx_t* ctx, uint32_t id) {


// #define SYS_YIELD     ( 0x00 )
// #define SYS_WRITE     ( 0x01 )
// #define SYS_READ      ( 0x02 )
// #define SYS_FORK      ( 0x03 )
// #define SYS_EXIT      ( 0x04 )
// #define SYS_EXEC      ( 0x05 )
// #define SYS_KILL      ( 0x06 )
// #define SYS_NICE      ( 0x07 )
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

    case 0x03 :{//FORK
//         PL011_putc( UART0, '[', true );
//         PL011_putc( UART0, 'F', true );
//         PL011_putc( UART0, 'O', true );
//         PL011_putc( UART0, 'R', true );
//         PL011_putc( UART0, 'K', true );
//         PL011_putc( UART0, ']', true );

        //increase console priority
        pcb[0].priority += 1;
        
        //get next PCB
        pcb_t* child = getNextPCB();

        //if there no available PCBs, return and do nothing
        if(child == NULL){
//            ctx->gpr[0] = current->pid;
            break;
        }
        
        //clone parents processing context
        memcpy(&child->ctx, ctx, sizeof(ctx_t));


        //activate PCB
        child->status = STATUS_CREATED;

        //clone parent stack and SP in child
        uint32_t sp_offset = (uint32_t) &current->tos - ctx->sp;
        child->ctx.sp = child->tos - sp_offset;
        memcpy((void *)child->ctx.sp, (void *)ctx->sp, sp_offset);

        //set return values
        child->ctx.gpr[0] = 0;
        ctx->gpr[0] = child->pid;

        break;
    }

    case 0x04:{//EXIT
//         PL011_putc( UART0, '[', true );
//         PL011_putc( UART0, 'E', true );
//         PL011_putc( UART0, 'X', true );
//         PL011_putc( UART0, 'I', true );
//         PL011_putc( UART0, 'T', true );
//         PL011_putc( UART0, ']', true );
        pcb[current->pid].status = STATUS_TERMINATED;
        schedule(ctx);
        pcb[0].priority -= 1;

        break;
    }

    case 0x05 :{//EXEC
//       PL011_putc( UART0, '[', true );
//       PL011_putc( UART0, 'E', true );
//       PL011_putc( UART0, 'X', true );
//       PL011_putc( UART0, 'E', true );
//       PL011_putc( UART0, 'C', true );
//       PL011_putc( UART0, ']', true );
      //set return values
      current->priority = 1;
      ctx->pc = ctx->gpr[0];
      break;
    }
    case 0x06 :{//KILL
//       PL011_putc( UART0, '[', true );
//       PL011_putc( UART0, 'K', true );
//       PL011_putc( UART0, 'I', true );
//       PL011_putc( UART0, 'L', true );
//       PL011_putc( UART0, 'L', true );
//       PL011_putc( UART0, ']', true );

      int pid = ctx->gpr[0];
      if(pid == -1){
          for(int i = 1; i < 20; i++){
            pcb[i].status = STATUS_TERMINATED;
          }
          pcb[0].priority = 2;
      }
      else if(pid >= 0 && pid < 20){
          pcb[pid].status = STATUS_TERMINATED;
          pcb[0].priority -= 1;
      }
      schedule(ctx);
      break;
    }
  case 0x07 :{//NICE
//       PL011_putc( UART0, '[', true );
//       PL011_putc( UART0, 'N', true );
//       PL011_putc( UART0, 'I', true );
//       PL011_putc( UART0, 'C', true );
//       PL011_putc( UART0, 'E', true );
//       PL011_putc( UART0, ']', true );
        
      int pid = ctx->gpr[0];
      int priority = ctx->gpr[1];
      if(pid >= 0 && pid < 20 && priority >= 0 && priority <= 20){
          pcb[pid].priority = priority;
          schedule(ctx);
      }
      break;
    }
      case 0x08:{//PS
          print_to_console("\n\tPID\tPRIORITY\n", 16);
          for(int i = 0; i < 20; i++){
              if(pcb[i].status != STATUS_TERMINATED){
                char pid[2];
                char priority[2];
                itoa(pid, pcb[i].pid);
                itoa(priority, pcb[i].priority);
                print_to_console("\t", 1);
                print_to_console(pid, 2);
                print_to_console("\t", 1);
                print_to_console(priority, 2);
                print_to_console("\n", 1);
              }
          }
          print_to_console("\n", 1);

          break;

      }
    case 0x09:{//PS
          print_to_console("\n", 1);
          for(int i = 0; i < file_table_size; i++){
              uint8_t* file;
              disk_rd(0x00000001+i, file, 16);
              //if not empty
              if(0 != strcmp(file,"0000000000000000")){
                print_to_console(file, 16);
                print_to_console("\n", 1);
              }
          }
          print_to_console("\n", 1);
          break;

      }
          
          

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }

  return;
}
