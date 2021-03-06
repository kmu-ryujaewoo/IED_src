#include<Servo.h>

// Arduino pin assignment
#define PIN_LED 9 // [1234] LED를 아두이노 GPIO 9번 핀에 연결
#define PIN_SERVO 10 // [1352] 서보모터를 아두이노의 10번 핀에 연결
#define PIN_IR A0 // [1352] IR센서를 아두이노 A0 핀에 연결

// Framework setting
#define _DIST_TARGET 255 // [1352]목표값을 탁구공 중심 위치까지 거리 255mm로 Fix
#define _DIST_MIN 100 // [1352] 최소 측정 거리를 100mm로 설정
#define _DIST_MAX 450 // [1352] 측정 거리의 최댓값을 410mm로 설정

// Distance sensor
#define _DIST_ALPHA 0.4
#define _SERVO_ALPHA 0.6

// Servo range
#define _DUTY_MIN 1100 //[2998] Servo Minimum microseconds
#define _DUTY_NEU 1400 //[2983] Servo 중간값 설정
#define _DUTY_MAX 1700 // [2979] Servo Max값 설정
#define _ITERM_MAX 80

// Servo speed control
#define _SERVO_ANGLE 30 // [2992] 서보 각 설정
#define _SERVO_SPEED 400 // [2976] 서보 스피드 설정
// Event periods
#define _INTERVAL_DIST 20 // [2987] 거리측정 INTERVAL값 설정
#define _INTERVAL_SERVO 20 // [2980] 서보 INTERVAL값 설정
#define _INTERVAL_SERIAL 100 //[2989] 시리얼 출력 INTERVAL 설정

// PID parameters
#define _KP 1.4
#define _KI 0.02
#define _KD 45

//calibrate sensor base value
#define DIST_10C 130 //[2998] sample value for sensor 10cm dist value for calibrate
#define DIST_40C 340 //[2998] sample value for sensor 40cm dist value for calibrate

// Servo instance
Servo myservo; //[2991] create servo object to control a servo

// Distance sensor
float dist_target; // location to send the ball [2976] 공 위치
float dist_min, dist_max, dist_raw, dist_ema, dist_prev;// [2981] 거리 변수 설정(현재, ema 필터 적용, 직전값) 

// Event periods
unsigned long last_sampling_time_dist, last_sampling_time_servo, last_sampling_time_serial; 
bool event_dist, event_servo, event_serial; //[2989] 논리형 변수 설정
unsigned long time_curr;

// Servo speed control
int duty_chg_per_interval; //[1352] 주기동안 duty 변화량 변수 
int duty_target, duty_curr;//[1352] 서보모터의 목표위치, 서보에 실제 입력할 위치

// PID variables
float error_curr, error_prev, control, pterm, dterm, iterm = 0;

float duty_ema = _DUTY_NEU;
double Time = 20;
double _kd, _ki, _kp;
double alpha;

void setup() {

   // initialize GPIO pins for LED and attach servo
   pinMode(PIN_LED, OUTPUT); 
   myservo.attach(PIN_SERVO); //[2998] Servo Attaching 
   myservo.writeMicroseconds(_DUTY_NEU); //[2998] Servo Set to neutral position

   // initialize global variables
   dist_min = _DIST_MIN; //[2999] dist_min 값 적용
   dist_max = _DIST_MAX; //[2999] dist_max 값 적용
   dist_ema = 0.0; //[2999] dist_ema 값 초기화
   alpha = _DIST_ALPHA; //[2999] alpha 선언 및 값 적용

   _kp = _KP;
   _ki = _KI;
   _kd = _KD;

   duty_curr = _DUTY_NEU;

   // convert angle speed into duty change per interval.
   duty_chg_per_interval = (_DUTY_MAX - _DUTY_MIN) * ((float)_SERVO_SPEED / 180) * ((float)_INTERVAL_DIST / 1000); //[2985] duty_chg_per_interval 설정

   
   // initialize serial port
   Serial.begin(57600); //[2999] serial port 57600

}
  

void loop() {

    time_curr = millis();

    if (time_curr >= last_sampling_time_dist + _INTERVAL_DIST){
        last_sampling_time_dist += _INTERVAL_DIST;
        event_dist = true;
    }
    if (time_curr >= last_sampling_time_servo + _INTERVAL_SERVO){
        last_sampling_time_servo += _INTERVAL_SERVO;
        event_servo = true;
    }
    if (time_curr >= last_sampling_time_serial + _INTERVAL_SERIAL){
        last_sampling_time_serial += _INTERVAL_SERIAL;
        event_serial = true;
    }


    if(event_dist) {
      event_dist = false;
      dist_raw = ir_distance() - 5;
      dist_ema = max(100, min(ir_distance_filtered(), 410));

     // PID control logic
     error_curr = _DIST_TARGET - dist_ema;
     pterm = _kp * error_curr;
     iterm += _ki * error_curr;
     dterm =  _kd * (error_curr - error_prev);
     
     if(pterm < 0) pterm *= 0.9;
     if(abs(pterm) > 5 and abs(pterm) < 75) pterm += (abs(pterm)/pterm*18);
     
     if(abs(dterm) > 135 and abs(pterm) > 5) dterm *= 2.5;
     else if(abs(error_curr) < 5) dterm /= 2;

     if(abs(error_curr) > 25) iterm = 0;
     if(iterm > _ITERM_MAX) iterm = _ITERM_MAX;
     else if(iterm < -_ITERM_MAX) iterm = -_ITERM_MAX;

     control = pterm + iterm + dterm;
     
     if(abs(pterm) > 5 and control < 0) control *= 0.9;
     
     error_prev = error_curr;
     duty_target = _DUTY_NEU + control;

     if (duty_target < _DUTY_MIN) {  //[1352] 조건일때 duty_target값을 _DUTY_MIN으로
         duty_target = _DUTY_MIN; 
     }
     else if (duty_target > _DUTY_MAX) { //[1352] 조건일때 duty_target값을 _DUTY_MAX으로
         duty_target = _DUTY_MAX;
     }
   }
  
   if(event_servo) {
      event_servo = false;
      duty_curr = func(duty_curr, duty_target);
      myservo.writeMicroseconds(duty_curr);
  }
  
  if(event_serial) {
     event_serial = false;
     Serial.print("IR:");
     Serial.print(dist_ema);
     Serial.print(",T:"); 
     Serial.print(dist_target);
     Serial.print(",P:");
     Serial.print(map(pterm,-1000,1000,510,610));
     Serial.print(",D:");
     Serial.print(map(dterm,-1000,1000,510,610));
     Serial.print(",I:");
     Serial.print(map(iterm,-1000,1000,510,610));
     Serial.print(",DTC:");
     Serial.print(map(duty_curr,1000,2000,410,510));
     Serial.println(", -G:245,+G:265,G:255,m:0,M:800");
  }
}
float ir_distance(void){ // return value unit: mm
    float val;
    float volt = float(analogRead(PIN_IR));
    val = ((6762.0/(volt-9.0))-4.0) * 10.0;
    val = (val - 70) / (480 - 70) * 350 + 100;
    return val;
}

float ir_distance_filtered(void){ 
    return (alpha*ir_distance())+((1-alpha)*dist_ema);
}

int func(int curr, int target){
    if((curr - target) > duty_chg_per_interval)
        return int(curr - duty_chg_per_interval);
    else if((target - curr) > duty_chg_per_interval)
        return int(curr + duty_chg_per_interval);
    else
        return target;
}
