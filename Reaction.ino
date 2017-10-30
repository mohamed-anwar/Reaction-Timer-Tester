/*
 *** Configuration Definitions ***
 * 0 Means Disabled
 * Any other value means enabled
 */

#define CONFIG_USE_INTERRUPT    1
#define CONFIG_USE_LCD          1
#define CONFIG_USE_EEPROM       1
#define CONFIG_USE_TIMER        1

void fsm_accept(int);

#define BUTTON  2
#define LED     13

#if CONFIG_USE_EEPROM
#include <EEPROM.h>
typedef union {
  byte bytes[4];
  unsigned long value;
} __attribute__((packed)) __ulong_t;

unsigned long get_best_time() {
  __ulong_t best_time;

  best_time.bytes[0] = EEPROM[0];
  best_time.bytes[1] = EEPROM[1];
  best_time.bytes[2] = EEPROM[2];
  best_time.bytes[3] = EEPROM[3];

  return best_time.value;
}

void set_best_time(unsigned long value) {
  __ulong_t best_time = {
    .value = value
  };

  EEPROM[0] = best_time.bytes[0];
  EEPROM[1] = best_time.bytes[1];
  EEPROM[2] = best_time.bytes[2];
  EEPROM[3] = best_time.bytes[3];
}
#endif  /* CONFIG_USE_EEPROM */

#if CONFIG_USE_TIMER
static bool wait_interval_finished = false;
#else
static unsigned long wait_start_time = 0;
#endif /* CONFIG_USE_TIMER */
static unsigned long respond_start_time = 0;

#if CONFIG_USE_TIMER
void timer1_init_ctc() {
  noInterrupts();

  /* Reset timer */
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  /* Use CTC mode */
  TCCR1B = _BV(WGM12);

  /* Set TOP to 249 (250000/1000 = 250 = 249 + 1 (Count starts from 0)) */
  OCR1A = 249;

  interrupts();
}

static unsigned long timer1_wait_interval = 0;
void timer1_wait(unsigned long interval) {

  /* Start counting from BOTTOM */
  TCNT1 = 0;

  /* Enable interrupt on compare match on channel B */
  TIMSK1 = _BV(OCIE1A);

  timer1_wait_interval = interval;

  /* Set prescaler to 64 (16MHz/64 = 250.00KHz) -- Starts the timer */
  TCCR1B |= _BV(CS11) | _BV(CS10);
}

SIGNAL(TIMER1_COMPA_vect) { /* Fires each (roughly) 1mS */

  if (!timer1_wait_interval) {
    /* Disable timer */
    TCCR1A = 0;
    TCCR1B = 0;
    wait_interval_finished = true;
    fsm_accept(0);
  }

  --timer1_wait_interval;
}
#endif /* CONFIG_USE_TIMER */

/* 
 * Liquid Crystal Display (LCD) Definitions and Includes
 */

#if CONFIG_USE_LCD

/* Number of columns in LCD */
#define LCD_COLS  16

/* Number of rows in LCD */
#define LCD_ROWS  2

/* LCD Interface pins */
#define LCD_RS  12
#define LCD_EN  11
#define LCD_D4  6
#define LCD_D5  5
#define LCD_D6  4
#define LCD_D7  3

/* Include library header 
 * (Arduino IDE/arduino-builder) automatically links with LiquidCrystal library
 * */
#include <LiquidCrystal.h>

/* Initialize LCD Module */
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
#endif  /* CONFIG_USE_LCD */

/*
 * Message showing (Allows both Serial and LCD viewing)
 */
void msg(char *msg) {
  Serial.println(msg);
#if CONFIG_USE_LCD
  lcd.print(msg);
#endif  /* CONFIG_USE_LCD */
}

/*
 * Finite State Machine model
 */
static enum {
  STATE_IDLE,
  STATE_READY,
  STATE_WAIT,
  STATE_CHEAT,
  STATE_RESPOND,
} state = STATE_IDLE;
static unsigned long random_wait_interval = 0;

void fsm_accept(int input) {
  switch (state) {
    case STATE_IDLE:
      /* Wait until user pushes the button. */
      if (input) {
        state = STATE_READY;
#if CONFIG_USE_LCD
        lcd.clear();
#endif
        msg("READY");
      }
      break;
    case STATE_READY:
      /* Blink the LED twice in 0.5 Sec, set the random counter
       * and record current time.
       * */
      noInterrupts();
      random_wait_interval = (unsigned long) random(1000, 5000);
      digitalWrite(LED, HIGH);
      _delay_ms(125);
      digitalWrite(LED, LOW);
      _delay_ms(125);
      digitalWrite(LED, HIGH);
      _delay_ms(125);
      digitalWrite(LED, LOW);
      _delay_ms(125);
      state = STATE_WAIT;
#if CONFIG_USE_TIMER
      timer1_init_ctc();
      timer1_wait(random_wait_interval);
#else /* Use Arduino functions */
      wait_start_time = millis();
#endif /* CONFIG_USE_TIMER */
      interrupts();
      break;
    case STATE_WAIT:
#if CONFIG_USE_TIMER
      if (wait_interval_finished) {
        wait_interval_finished = false;
#else   /* Use Arduino functions */
      if (millis() - wait_start_time >= random_wait_interval) {
#endif  /* CONFIG_USE_TIMER */
        digitalWrite(LED, HIGH);
        respond_start_time = millis();
        state = STATE_RESPOND;
      } else if (input) {
#if CONFIG_USE_LCD
        lcd.clear();
#endif
        msg("CHEAT");
        analogWrite(LED, 25);
        state = STATE_CHEAT;
      }
      break;
    case STATE_RESPOND:
      if (input) {
        unsigned long respond_time = millis() - respond_start_time;
        char buf[50] = {0};
#if CONFIG_USE_LCD
        lcd.clear();
#endif
        snprintf(buf, 50, "TIME: %lumS", respond_time);
        msg(buf);
#if CONFIG_USE_EEPROM
        unsigned long best_time = get_best_time();
        if (respond_time < best_time)
          set_best_time(respond_time);
        snprintf(buf, 50, "BEST: %lumS", best_time);
#if CONFIG_USE_LCD
        lcd.setCursor(0, 1);
#endif  /* CONFIG_USE_LCD */
        msg(buf);
#if CONFIG_USE_LCD
        lcd.setCursor(0, 0);
#endif  /* CONFIG_USE_LCD */
#endif  /* CONFIG_USE_EEPROM */
        digitalWrite(LED, LOW);
        state = STATE_IDLE;
      }
      break;
    case STATE_CHEAT:
#if CONFIG_USE_TIMER
      if (wait_interval_finished) {
        wait_interval_finished = false;
#else /* Use Arduino functions */
      if (millis() - wait_start_time >= random_wait_interval) {
#endif  /* CONFIG_USE_TIMER */
        digitalWrite(LED, HIGH);
        respond_start_time = millis();
        state = STATE_RESPOND;
      } else if (input) {
#if CONFIG_USE_LCD
        lcd.clear();
#endif
        msg("CHEAT TWICE");
        digitalWrite(LED, LOW);
        state = STATE_IDLE;
      }
      break;
  }
}

#if CONFIG_USE_INTERRUPT
/* Interrupt Service Routine -- Attached to INT0 Pin */
void input_isr(void) {
  static int input = 0;
  static int __last_input = 0;
  input = digitalRead(BUTTON);
  int accept = input == __last_input? 0 : input;
  fsm_accept(accept);
  __last_input = input;
}
#endif  /* CONFIG_USE_INTERRUPT */

void setup() {
  randomSeed(analogRead(0));    /* Initialize Random Seed */
  Serial.begin(9600);
  pinMode(BUTTON, INPUT);
  pinMode(LED, OUTPUT);
#if CONFIG_USE_INTERRUPT
  attachInterrupt(digitalPinToInterrupt(BUTTON), input_isr, CHANGE);
#endif  /* CONFIG_USE_INTERRUPT */

#if CONFIG_USE_LCD
  lcd.begin(LCD_ROWS, LCD_COLS);
  lcd.display();
#endif
}

void loop() {
#if CONFIG_USE_INTERRUPT
#if CONFIG_USE_TIMER
  /* Seriously, just do nothing! */
#else
  fsm_accept(0);
#endif
#else /* Use polling */
  int input = 0;
  static int __last_input = 0;
  input = digitalRead(BUTTON);
  int accept = input == __last_input? 0 : input;
  fsm_accept(accept);
  __last_input = input;
#endif  /* !CONFIG_USE_INTERRUPT */
}
