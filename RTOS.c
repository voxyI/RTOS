/**
 * USAGE:
 * Set up TCBs and stacks for each thread as globals
 * ex:  TCB t1, t2, t3;
 *      uint32_t s1[STACK_SIZE];
 *      uint32_t s2[STACK_SIZE];
 *      uint32_t s3[STACK_SIZE];
 *      
 * In main, chain together the TCBs
 *      t1.next = &t2;
 *      t2.next = &t3;
 *      t3.next = &t1;
 * 
 * set next_tcb to the first thread
 * 
 * Set a timer interrupt to call SysTick_Handler on desired interval
 */


typedef struct TCB {
	uint32_t *stack_pointer;
	struct TCB *next;
} TCB;


TCB volatile *next_tcb = NULL;
TCB volatile *current_tcb = NULL;


/**
 * @brief initializes a thread and configures a "fake" stack for the process"
 * 
 * @param tcb
 *      A TCB struct for the function to create a thread out of
 * 
 * @param stack_array
 *      The stack to be edited
 * 
 * @param stack_size
 *      The size of the stack
 * 
 * @param task
 *      The function to create a thread out of
 */
void OS_ThreadInit(TCB *tcb, uint32_t *stack_array, int stack_size, void (*task)(void)) {
    // 1. Initialize 'sp' to the END of the array (grows downward on ARM Cortex-M)
    uint32_t *sp = &stack_array[stack_size];

    // 2. Push the registers
    *(--sp) = 0x01000000; //xPSR
    *(--sp) = (uint32_t) task; //PC
    *(--sp) = 0; //LR -> errorHandler
    *(--sp) = 0; //R12
    *(--sp) = 0; //R3
    *(--sp) = 0; //R2
    *(--sp) = 0; //R1
    *(--sp) = 0; //R0
    *(--sp) = 0; //R11
    *(--sp) = 0; //R10
    *(--sp) = 0; //R9
    *(--sp) = 0; //R8
    *(--sp) = 0; //R7
    *(--sp) = 0; //R6
    *(--sp) = 0; //R5
    *(--sp) = 0; //R4
    
    // 3. Save the final 'sp' into the TCB
    tcb->stack_pointer = sp;
}


/**
 * @brief
 *      Handles the timer interrupts by setting a flag for PendSV so the context switch happens at a low priority.
 * 
 * Should not be explicitally called, only called through interrupts
 */
void SysTick_Handler(void) {
    next_tcb = current_tcb->next;
    SCB->ICSR |= (1UL<<28);
}


/**
 * @brief
 *      Handles the context swtiching between threads.
 *      
 * Should not be explicitally called, only called through interrupts
 */
__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        "LDR R1, =current_tcb \n"     // Get address of current_tcb
        "LDR R2, [R1] \n"             // Load the actual pointer value
        
        "CMP R2, #0 \n"               // Is current_tcb == NULL?
        "BEQ LoadNewContext \n"       // If yes, skip saving 
        // ----------------------------------
        "MRS R0, PSP \n"
        "STMDB R0!, {R4-R11} \n"
        "STR R0, [R2] \n"

        "LoadNewContext: \n"          // Label to jump to
        // --- LOAD CONTEXT ---
        "LDR R3, =next_tcb \n"
        "LDR R4, [R3] \n"
        "STR R4, [R1] \n"             // current_tcb = next_tcb

        "LDR R0, [R4] \n"
        "LDMIA R0!, {R4-R11} \n"
        
        "MSR PSP, R0 \n"
        "ISB \n" //should be done automatically
        
        // Ensure we return to Thread Mode using PSP
        // write 0xFFFFFFFD to LR to tell the CPU to use the process stack
        "LDR R0, =0xFFFFFFFD \n"
        "MOV LR, R0 \n"
        
        "BX LR \n"
    );
}
