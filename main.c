#include <mega128.h>
#include <delay.h>
#define BAUD 9600  // Desired baud rate
#define F_CPU 16000000UL  // MCU clock frequency
#define BAUD_PRESCALE ((F_CPU / (16UL * BAUD)) - 1)  // Calculate baud prescaler value
#define MAX_FRONT 730
#define STEP_FRONT 0
#define STEP_BACK 1
#define STEP_DIR 3
#define STEP_STEP 2
unsigned char receivedData;
unsigned char state = 0;
unsigned char xHighByte, xLowByte;
unsigned int xCoordinate;
unsigned char yHighByte, yLowByte;
unsigned int yCoordinate,cnt;

// Function to control the stepper motor
void step_motor(int steps, int delay, int direction) {
   int i;
   // Set the direction
   if (direction == 1)
      PORTD.3 = 1;
   else
      PORTD.3 = 0;

   // Step the motor
   for (i = 0; i < steps; i++) {
      PORTD.2 = 1;
      delay_ms(delay);
      PORTD.2 = 0;
      delay_ms(delay);
   }
}                 
     
unsigned char USART_Receive(void) {
    // Wait for data to be received
    while (!(UCSR0A & (1<<RXC0)));
    // Get and return received data from buffer
    return UDR0;
}

void main(void) {      
    UBRR0H = (unsigned char)(BAUD_PRESCALE >> 8);
    UBRR0L = (unsigned char)BAUD_PRESCALE;
    DDRC = 0xFF;
    DDRD = 0xFF;
    DDRB |= 0xFF;
    PORTB |= 0xFF;
    TCCR1A = 0x82;
    TCCR1B = 0x1B;
    ICR1 = 4999; // TOP
    OCR1A = 221; // 0 degree
    yCoordinate = 0;    
       
    delay_ms(2000);

    // Enable receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    // Set frame format: 8 data bits, no parity, 1 stop bit
    UCSR0C = (3 << UCSZ00);
    
    while (1) {
        receivedData = USART_Receive();
        if (state == 0) {
            if (receivedData == 0xFF) {
                step_motor(yCoordinate , 2, STEP_BACK);
                state = 1;
                OCR1A = 221;                 
                PORTC = 0x00;
            }
        } else if (state == 1) {
            xHighByte = receivedData;  // Save x-coordinate high byte 
            state = 2;  // Move to the next state
             
        } else if (state == 2) {
            xLowByte = receivedData;  // Save x-coordinate low byte
            xCoordinate = ((unsigned int)xHighByte << 8) | xLowByte;
            
            xCoordinate = 221 + (int)((1920-xCoordinate)/1920.0*47); 
            OCR1A = xCoordinate;      
            state = 3;  // Reset the state to start again   
        }
        
        else if (state == 3) {
            yHighByte = receivedData;  // Save y-coordinate high byte         
            state = 4;  // Move to the next state
        } else if (state == 4) {    
            yLowByte = receivedData;  // Save x-coordinate low byte
            yCoordinate = ((unsigned int)yHighByte << 8) | yLowByte;
             
            yCoordinate = (int)(yCoordinate/1080.0*730);
            
            if(yCoordinate<=623)
                yCoordinate += 107;
                        
            step_motor(yCoordinate, 2, STEP_FRONT);            
            state = 0;         
            UDR0 = 0xEE;  
        }                  
    } 
}