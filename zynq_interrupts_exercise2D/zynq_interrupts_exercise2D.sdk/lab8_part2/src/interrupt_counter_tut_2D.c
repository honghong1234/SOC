
#include "xparameters.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"

// Parameter definitions
#define INTC_DEVICE_ID 		XPAR_PS7_SCUGIC_0_DEVICE_ID
#define TMR_DEVICE_ID		XPAR_TMRCTR_0_DEVICE_ID
#define BTNS_DEVICE_ID		XPAR_AXI_GPIO_0_DEVICE_ID
#define LEDS_DEVICE_ID		XPAR_AXI_GPIO_1_DEVICE_ID
#define SWHS_DEVICE_ID  	XPAR_AXI_GPIO_2_DEVICE_ID
#define INTC_GPIO_INTERRUPT_ID XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define INTC_TMR_INTERRUPT_ID XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR

#define BTN_INT 			XGPIO_IR_CH1_MASK
#define TMR_LOAD			0xFF000000

#define RESETBUTTON 0b0100
#define STARTBUTTON 0b0010
#define LEFTPADDLE 0b1000
#define RIGHTPADDLE 0b0001
#define LEDOUT 0b0000
#define LED_NUMBER 4
#define LEFT 0
#define RIGHT 1
#define LEFTPOSITION 0
#define RIGHTPOSITION 3

char StartDirection;

XGpio LEDInst, BTNInst, SWHInst;
XScuGic INTCInst;
XTmrCtr TMRInst;
static int btn_value;

int LedPosition, round_over;
int swh_value, swh_value_prev;
int scoreright, scoreleft;
int LED_PATTERNS[LED_NUMBER]={0b1000, 0b0100, 0b0010,0b0001};

// PROTOTYPE FUNCTIONS
void PressMistake(void);
static void BTN_Intr_Handler(void *baseaddr_p);
static void TMR_Intr_Handler(void *baseaddr_p);
static int InterruptSystemSetup(XScuGic *XScuGicInstancePtr);
static int IntcInitFunction(u16 DeviceId, XTmrCtr *TmrInstancePtr, XGpio *GpioInstancePtr);

//----------------------------------------------------
// INTERRUPT HANDLER FUNCTIONS
// - called by the timer, button interrupt, performs
// - LED flashing
//----------------------------------------------------

void BTN_Intr_Handler(void *InstancePtr)
{
	// Disable GPIO interrupts
	XGpio_InterruptDisable(&BTNInst, BTN_INT);
	// Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&BTNInst) & BTN_INT) != BTN_INT) {
			return;
	}

	btn_value = XGpio_DiscreteRead(&BTNInst, 1);
	if(btn_value == RESETBUTTON) { //reset game
			xil_printf("\n\rNew Game - Scores Reset\r\n");
			xil_printf("Score Left = %d Score Right = %d\r\n",  scoreleft, scoreright);

			scoreright = 0; scoreleft = 0;
			//Todo: after reset
	}

	if(btn_value == STARTBUTTON) {
		if(round_over) {
			round_over = 0;
		}
		XGpio_DiscreteWrite(&LEDInst, 1, LED_PATTERNS[LedPosition]);
	}

	else {
		if(!round_over) {
			if(StartDirection == RIGHT) {
				if(btn_value == RIGHTPADDLE) {
					if(LED_PATTERNS[LedPosition] != RIGHTPADDLE ) {
						scoreleft += 1;
						round_over = 1;
						xil_printf("    Early Press  \r\n");
						PressMistake();
					}
					else {  //set StartDirection
						StartDirection = LEFT;
					}
				}
			}
			else {
				if(btn_value == LEFTPADDLE) {
					if(LED_PATTERNS[LedPosition] != LEFTPADDLE ) {
						scoreright += 1;
						round_over = 1;
						xil_printf("    Early Press\r\n");
						PressMistake();
					}
					else {  //set StartDirection
						StartDirection = RIGHT;
					}
				}
			}
		}

	}

    (void)XGpio_InterruptClear(&BTNInst, BTN_INT);
    // Enable GPIO interrupts
    XGpio_InterruptEnable(&BTNInst, BTN_INT);
}

void TMR_Intr_Handler(void *data)
{
	if(round_over) {
		XTmrCtr_Stop(&TMRInst,0);
		XTmrCtr_Reset(&TMRInst,0);
		XTmrCtr_Start(&TMRInst,0);
		return;
	}

	if (XTmrCtr_IsExpired(&TMRInst,0)){
		// Once timer has expired, stop, increment counter
		// reset timer and start running again
		XTmrCtr_Stop(&TMRInst,0);
		if(StartDirection == RIGHT) {
			if(LedPosition == RIGHTPOSITION){
				scoreleft ++;
				round_over = 1;
				xil_printf("    Right Player Loss	\r\n");
				PressMistake();
			}
			else {
				LedPosition ++;
			}
		}
		else {
			if(LedPosition == LEFTPOSITION){
				scoreright ++;
				round_over = 1;
				xil_printf("    Left Player Loss	\r\n");
				PressMistake();
			}
			else {
				LedPosition --;
			}
		}
		if(!round_over)
			XGpio_DiscreteWrite(&LEDInst, 1, LED_PATTERNS[LedPosition]);
		XTmrCtr_Reset(&TMRInst,0);
		XTmrCtr_Start(&TMRInst,0);
	}
}

void PressMistake(void) {
	if(StartDirection == LEFT) {
		LedPosition = LEFTPOSITION;
		StartDirection = RIGHT;
	}
	else {
		LedPosition = RIGHTPOSITION;
		StartDirection = LEFT;
	}
	xil_printf("Score Left = %d Score Right = %d\r\n",  scoreleft, scoreright);
	XGpio_DiscreteWrite(&LEDInst, 1, LEDOUT);
}


int main (void)
{
  int status;
  // Initialise
  status = XGpio_Initialize(&LEDInst, LEDS_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;

  status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;

	status = XGpio_Initialize(&SWHInst, SWHS_DEVICE_ID);
	if(status != XST_SUCCESS) return XST_FAILURE;
  // Set direction
  XGpio_SetDataDirection(&LEDInst, 1, 0x00);
  XGpio_SetDataDirection(&BTNInst, 1, 0xFF);
	XGpio_SetDataDirection(&SWHInst, 1, 0xFF);

  // SETUP THE TIMER
  status = XTmrCtr_Initialize(&TMRInst, TMR_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;
  XTmrCtr_SetHandler(&TMRInst, TMR_Intr_Handler, &TMRInst);
  XTmrCtr_SetResetValue(&TMRInst, 0, TMR_LOAD);
  XTmrCtr_SetOptions(&TMRInst, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);

  // Initialize interrupt controller
  status = IntcInitFunction(INTC_DEVICE_ID, &TMRInst, &BTNInst);
  if(status != XST_SUCCESS) return XST_FAILURE;

  XTmrCtr_Start(&TMRInst, 0);

  xil_printf("Start of the Ping Pong Program\r\n");
  while(1) {
		swh_value = XGpio_DiscreteRead(&SWHInst, 1);
		if (swh_value != swh_value_prev) {
			xil_printf("Switch Status Changed!\r\n");
			swh_value_prev = swh_value;
			 // load timer with the new switch settings
			 XTmrCtr_Stop(&TMRInst,0);
			 XTmrCtr_SetResetValue(&TMRInst, 0, TMR_LOAD*swh_value);
			 XTmrCtr_Start(&TMRInst, 0);
		}
	}

  return 0;
}

//----------------------------------------------------
// INITIAL SETUP FUNCTIONS
//----------------------------------------------------
int InterruptSystemSetup(XScuGic *XScuGicInstancePtr)
{
	// Enable interrupt
	XGpio_InterruptEnable(&BTNInst, BTN_INT);
	XGpio_InterruptGlobalEnable(&BTNInst);

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 	 	 	 	 	 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
			 	 	 	 	 	 XScuGicInstancePtr);
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

int IntcInitFunction(u16 DeviceId, XTmrCtr *TmrInstancePtr, XGpio *GpioInstancePtr)
{
	XScuGic_Config *IntcConfig;
	int status;

	// Interrupt controller initialisation
	IntcConfig = XScuGic_LookupConfig(DeviceId);
	status = XScuGic_CfgInitialize(&INTCInst, IntcConfig, IntcConfig->CpuBaseAddress);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Call to interrupt setup
	status = InterruptSystemSetup(&INTCInst);
	if(status != XST_SUCCESS) return XST_FAILURE;
	
	// Connect GPIO interrupt to handler
	status = XScuGic_Connect(&INTCInst,
					  	  	 INTC_GPIO_INTERRUPT_ID,
					  	  	 (Xil_ExceptionHandler)BTN_Intr_Handler,
					  	  	 (void *)GpioInstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Connect timer interrupt to handler
	status = XScuGic_Connect(&INTCInst,
							 INTC_TMR_INTERRUPT_ID,
							 (Xil_ExceptionHandler)TMR_Intr_Handler,
							 (void *)TmrInstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Enable GPIO interrupts interrupt
	XGpio_InterruptEnable(GpioInstancePtr, 1);
	XGpio_InterruptGlobalEnable(GpioInstancePtr);

	// Enable GPIO and timer interrupts in the controller
	XScuGic_Enable(&INTCInst, INTC_GPIO_INTERRUPT_ID);
	XScuGic_Enable(&INTCInst, INTC_TMR_INTERRUPT_ID);
	
	return XST_SUCCESS;
}

