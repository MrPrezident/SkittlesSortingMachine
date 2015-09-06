#include <MD_TCS230.h>
#include <FreqCount.h>

#include <Wire.h>
#include <Adafruit_MotorShield.h>

Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 
Adafruit_DCMotor motor[3] = {*AFMS.getMotor(1),*AFMS.getMotor(2),*AFMS.getMotor(3)};

#define UINT32_MAX 0xFFFFFFFF

#define  DEBUG_SKITTLES 1

#if  DEBUG_SKITTLES
#define  DUMP(s, v)  { Serial.print(F(s)); Serial.print(v); }
#define  DUMPS(s)    Serial.print(F(s))
#else
#define  DUMP(s, v)
#define  DUMPS(s)
#endif


#define TOPDISK 0
#define BOTDISK 1
#define HOLE false
#define SPACE true


#define  BLACK_CAL 0
#define WHITE_CAL 1
#define READ_VAL  2

#define NUM_COLORS 5
#define NUM_RGB 3
uint32_t skittles[NUM_COLORS][NUM_RGB] = {
                        {0,0,0},
                        {0,0,0},
                        {0,0,0},
                        {0,0,0},
                        {0,0,0}};
                        
float scale_factor[NUM_RGB] = {0.0, 0.0, 0.0};

// Interrupt definitions
#define TOPDISK_INT 0
#define BOTDISK_INT 1

// Pin definitions
#define  S0_OUT   8
#define  S1_OUT   9
#define  S2_OUT  10
#define  S3_OUT  11
#define TOPDISK_PIN 2
#define BOTDISK_PIN 3
#define TRIPWIRE_PIN 4
#define RED_BUTTON 13
#define GREEN_BUTTON 12

MD_TCS230 CS(S2_OUT, S3_OUT, S0_OUT, S1_OUT);

volatile uint8_t diskDir[2] = {FORWARD, FORWARD};
volatile uint8_t diskPos[2] = {0,0};
volatile uint8_t targetPos[2] = {0,0};

bool isStop = false;
bool isInitialized = false;

void set_abs_pos(uint8_t disk, uint8_t pos){
  diskPos[disk] = pos;
}

void set_pos(uint8_t disk, uint8_t pos, bool isSpace){
  diskPos[disk] = 2*pos + isSpace;
}

uint8_t get_abs_pos(uint8_t disk){
  return diskPos[disk];
}

void add_abs_pos(uint8_t disk, int num){
  diskPos[disk] = mod(diskPos[disk] + num, 10);
}

uint8_t get_pos(uint8_t disk){
  return diskPos[disk] / 2;
}

bool is_space(uint8_t disk){
  return ((diskPos[disk] % 2) == 1);
}


void set_abs_target_pos(uint8_t disk, uint8_t pos){
  targetPos[disk] = pos;
}

void set_target_pos(uint8_t disk, uint8_t pos, bool isSpace){
  targetPos[disk] = 2*pos + isSpace;
}

uint8_t get_abs_target_pos(uint8_t disk){
  return targetPos[disk];
}

void add_abs_target_pos(uint8_t disk, int num){
  targetPos[disk] = mod(targetPos[disk] + num, 10);
}


uint8_t get_target_pos(uint8_t disk){
  return targetPos[disk] / 2;
}

bool is_space_target(uint8_t disk){
  return ((targetPos[disk] % 2) == 1);
}

bool is_target(uint8_t disk){
  return (targetPos[disk] == diskPos[disk]);
}


void top_pos_handler(){
  int value = digitalRead(TOPDISK_PIN);
  
  if(value){
    if((diskDir[TOPDISK] == BACKWARD) && is_space(TOPDISK)){
      add_abs_pos(TOPDISK, -1);
    } else if((diskDir[TOPDISK] == FORWARD) && !is_space(TOPDISK)) {
      add_abs_pos(TOPDISK, 1);
    }
  } else {
    if((diskDir[TOPDISK] == BACKWARD) && !is_space(TOPDISK)){
      add_abs_pos(TOPDISK, -1);
    } else if((diskDir[TOPDISK] == FORWARD) && is_space(TOPDISK)) {
      add_abs_pos(TOPDISK, 1);
    }
  }
  
}

void bot_pos_handler(){
  int value = digitalRead(BOTDISK_PIN);
  
  if(value){
    if((diskDir[BOTDISK] == BACKWARD) && is_space(BOTDISK)){
      add_abs_pos(BOTDISK, -1);
    } else if((diskDir[BOTDISK] == FORWARD) && !is_space(BOTDISK)) {
      add_abs_pos(BOTDISK, 1);
    }
  } else {
    if((diskDir[BOTDISK] == BACKWARD) && !is_space(BOTDISK)){
      add_abs_pos(BOTDISK, -1);
    } else if((diskDir[BOTDISK] == FORWARD) && is_space(BOTDISK)) {
      add_abs_pos(BOTDISK, 1);
    }
  }
  
}


void setup()
{
  pinMode(RED_BUTTON, INPUT);     // set pin to input
  digitalWrite(RED_BUTTON, HIGH); // turn on pullup resistors
  pinMode(GREEN_BUTTON, INPUT);     // set pin to input
  digitalWrite(GREEN_BUTTON, HIGH); // turn on pullup resistors

  
  attachInterrupt(TOPDISK_INT, top_pos_handler, CHANGE);
  attachInterrupt(BOTDISK_INT, bot_pos_handler, CHANGE);
  
  Serial.begin(115200);
  DUMPS("Start skittles\n");
  
  CS.begin();

  
  AFMS.begin();  // create with the default frequency 1.6KHz


}

void loop()
{
  while(isStop){ if(digitalRead(GREEN_BUTTON) == LOW) { isStop = false; } }
  if(!isInitialized) { 
    calibrate_color();
    calibrate_positions();
  }

  drop_skittle();
  move_to_color_sensor();
  int index = readColor();
  
  DUMP("initial botPos is ", get_abs_pos(BOTDISK));
  DUMP("\ninitial topPos is ", get_abs_pos(TOPDISK));
  DUMPS("\n");
  
  go_both(index, HOLE);
  go_to(BOTDISK, index, SPACE);

  DUMP("final botPos is ", get_abs_pos(BOTDISK));
  DUMP("\nfinal topPos is ", get_abs_pos(TOPDISK));
  DUMPS("\n");

}

void drop_skittle(){
  if(isStop){ return; }
  
  if(get_abs_pos(BOTDISK) == 7 || get_abs_pos(BOTDISK) == 8){
    // move bottom disk away from feeder and color sensor
    go_to(BOTDISK, 4, SPACE);
  }
  if(is_space(TOPDISK)){
    // move to top disk to hole first if not already
    go(TOPDISK, FORWARD, 1, HOLE);
  }

  motor[2].setSpeed(255);
  motor[2].run(FORWARD);
  
  // Wait for Skittle
  unsigned long startTime = millis();
  while(analogRead(2) < 450){ 
    if(millis() > startTime + 5000) {
      motor[0].run(RELEASE);
      motor[1].run(RELEASE);
      motor[2].run(RELEASE);
      isStop = true;
      return;
    }
    if(isStop || digitalRead(RED_BUTTON) == LOW){
      motor[0].run(RELEASE);
      motor[1].run(RELEASE);
      motor[2].run(RELEASE);
      isStop = true;
      isInitialized = false;
      return;
    }
  }
  //while(digitalRead(TRIPWIRE_PIN) == HIGH){ }
  
  motor[2].run(RELEASE);
  
  // Wait briefly to make sure skittle has
  // fallen in hole before moving disk
  delay(250);

  // top disk position is now at feeder
  set_abs_pos(TOPDISK, 8);
}

void move_to_color_sensor() {
  go_to(TOPDISK, 3, SPACE);
}

void go_360(int disk, uint8_t dir, bool isSpace){
  if(isStop){ return; }
    
  motor[disk].setSpeed(255);
  runDisk(disk, dir);
  for(int i=0; i<5; i++){
   if(dir == FORWARD){
      set_target_pos(disk, mod(get_pos(disk) + 1, 5), isSpace);
    } else {
      set_target_pos(disk, mod(get_pos(disk) - 1, 5), isSpace);
    }
    while(!is_target(disk)){ if(isStop || digitalRead(RED_BUTTON) == LOW){ motor[0].run(RELEASE); motor[1].run(RELEASE); motor[2].run(RELEASE); isStop = true; isInitialized = false; return; } }
  }
  motor[disk].run(RELEASE);
}

void go(int disk, uint8_t dir, int num, bool isSpace){
  if(isStop){ return; }
    
  DUMP("go(", disk);
  DUMP(",", num);
  DUMP(",", isSpace);
  DUMPS(")\n");
  
 if(dir == FORWARD){
    go_to(disk, mod(get_pos(disk) + num, 5), isSpace);
  } else {
    go_to(disk, mod(get_pos(disk) - num, 5), isSpace);
  }
 
}

void go_to(int disk, uint8_t pos, bool isSpace){
  if(isStop){ return; }
  
  set_target_pos(disk, pos, isSpace);
  
  uint8_t dir;
  if(mod(get_abs_target_pos(disk) - get_abs_pos(disk), 10) > mod(get_abs_pos(disk) - get_abs_target_pos(disk), 10)){
    dir = BACKWARD;
  } else {
    dir = FORWARD;
  }
  
  motor[disk].setSpeed(255);
  runDisk(disk, dir);

    DUMP("go_to*", disk);
    DUMP("*", dir);
    DUMP("* (currentPos,targetPos) = (", get_abs_pos(disk));
    DUMP(",", get_abs_target_pos(disk));
    DUMPS(")\n");

   while(!is_target(disk)){ if(isStop || digitalRead(RED_BUTTON) == LOW){ motor[0].run(RELEASE); motor[1].run(RELEASE); motor[2].run(RELEASE); isStop = true; isInitialized = false; return; } }
   
   DUMPS("Reverse\n");
   
   add_abs_pos(disk, (dir == FORWARD) ? 1 : -1);
   if(dir==FORWARD){
     motor[disk].setSpeed(255);
   } else {
     motor[disk].setSpeed(180);
   }
   runDisk(disk, opposite_dir(dir));

   while(!is_target(disk)){ if(isStop || digitalRead(RED_BUTTON) == LOW){ motor[0].run(RELEASE); motor[1].run(RELEASE); motor[2].run(RELEASE); isStop = true; isInitialized = false; return; } }

   DUMPS("Reverse Again\n");

   add_abs_pos(disk, (dir == BACKWARD) ? 1 : -1);
   if(dir==FORWARD){
     motor[disk].setSpeed(180);
   } else {
     motor[disk].setSpeed(70);
   }
   runDisk(disk, dir);

   while(!is_target(disk)){ if(isStop || digitalRead(RED_BUTTON) == LOW){ motor[0].run(RELEASE); motor[1].run(RELEASE); motor[2].run(RELEASE); isStop = true; isInitialized = false; return; } }

   if(dir==FORWARD){
     DUMPS("Reverse Again!!!\n");
  
     add_abs_pos(disk, 1);
     motor[disk].setSpeed(70);
     runDisk(disk, opposite_dir(dir));
  
     while(!is_target(disk)){ if(isStop || digitalRead(RED_BUTTON) == LOW){ motor[0].run(RELEASE); motor[1].run(RELEASE); motor[2].run(RELEASE); isStop = true; isInitialized = false; return; } }
   }

   motor[disk].run(RELEASE);

   DUMPS("Done\n");
  
}

void go_both(uint8_t pos, bool isSpace){
  if(isStop){ return; }
  
  set_target_pos(TOPDISK, pos, isSpace);
  set_target_pos(BOTDISK, pos, isSpace);
  
  uint8_t dirTop, dirBot;
  if(mod(get_abs_target_pos(TOPDISK) - get_abs_pos(TOPDISK), 10) > mod(get_abs_pos(TOPDISK) - get_abs_target_pos(TOPDISK), 10)){
    dirTop = BACKWARD;
  } else {
    dirTop = FORWARD;
  }
  if(mod(get_abs_target_pos(BOTDISK) - get_abs_pos(BOTDISK), 10) > mod(get_abs_pos(BOTDISK) - get_abs_target_pos(BOTDISK), 10)){
    dirBot = BACKWARD;
  } else {
    dirBot = FORWARD;
  }
  
  DUMP("go_both*", dirTop);
  DUMP("*", dirBot);
  DUMP("* (currPosTop,curPosBot) = (", get_abs_pos(TOPDISK));
  DUMP(",", get_abs_pos(BOTDISK));
  DUMP(")  (targetPosTop,targetPosBot) = (", get_abs_target_pos(TOPDISK));
  DUMP(",", get_abs_target_pos(BOTDISK));
  DUMPS(")\n");
 
  
  int numTop = 0;
  int numBot = 0;
  motor[TOPDISK].setSpeed(255);
  motor[BOTDISK].setSpeed(255);
  runDisk(TOPDISK, dirTop);
  runDisk(BOTDISK, dirBot);
  while(numTop<4 || numBot<4){
    if(isStop || digitalRead(RED_BUTTON) == LOW){ motor[0].run(RELEASE); motor[1].run(RELEASE); motor[2].run(RELEASE); isStop = true; isInitialized = false; return; } 
     
    if(numTop<4 && is_target(TOPDISK)){
      numTop++;
      DUMP("numTop:", numTop); 
      DUMPS("\n"); 
      uint8_t dir;
      switch (numTop){
        case 1:
          DUMPS("TOP Reverse\n");
          dir = opposite_dir(dirTop);
          add_abs_pos(TOPDISK, (dir == FORWARD) ? -1 : 1);
          motor[TOPDISK].setSpeed(255);
          runDisk(TOPDISK, dir);
          break;
        case 2:
          DUMPS("TOP Reverse Again\n");
          dir = dirTop;
          add_abs_pos(TOPDISK, (dir == FORWARD) ? -1 : 1);
          motor[TOPDISK].setSpeed(150);
          runDisk(TOPDISK, dir);
          break;
        case 3:
          if(dirTop==FORWARD){
            DUMPS("TOP Reverse Again!!!\n");
            dir = opposite_dir(dirTop);
            add_abs_pos(TOPDISK, (dir == FORWARD) ? -1 : 1);
            motor[TOPDISK].setSpeed(150);
            runDisk(TOPDISK, dir);
          } else {
            numTop++;
      DUMP("numTop:", numTop); 
      DUMPS("\n"); 
            motor[TOPDISK].run(RELEASE);
            DUMPS("DoneTop\n");
          }
          break;
        default:
            motor[TOPDISK].run(RELEASE);
            DUMPS("DoneTop\n");          
      }
    }
    if(numBot<4 && is_target(BOTDISK)){
      numBot++;
      DUMP("numBot:", numBot); 
      DUMPS("\n"); 
      uint8_t dir;
      switch (numBot){
        case 1:
          DUMPS("BOT Reverse\n");
          dir = opposite_dir(dirBot);
          add_abs_pos(BOTDISK, (dir == FORWARD) ? -1 : 1);
          motor[BOTDISK].setSpeed(255);
          runDisk(BOTDISK, dir);
          break;
        case 2:
          DUMPS("BOT Reverse Again\n");
          dir = dirBot;
          add_abs_pos(BOTDISK, (dir == FORWARD) ? -1 : 1);
          motor[BOTDISK].setSpeed(150);
          runDisk(BOTDISK, dir);
          break;
        case 3:
          if(dirBot==FORWARD){
            DUMPS("BOT Reverse Again!!!\n");
            dir = opposite_dir(dirBot);
            add_abs_pos(BOTDISK, (dir == FORWARD) ? -1 : 1);
            motor[BOTDISK].setSpeed(150);
            runDisk(BOTDISK, dir);
          } else {
            numBot++;
      DUMP("numBot:", numBot); 
      DUMPS("\n"); 
            motor[BOTDISK].run(RELEASE);
            Serial.println("DoneBot");
          }
          break;
        default:
            motor[BOTDISK].run(RELEASE);
            DUMPS("DoneBot\n");          
          
      }
    }
  }
    
}

void calibrate_positions(){
  if(isStop){ return; }
    
  go(TOPDISK, BACKWARD, 1, SPACE);
  
  motor[BOTDISK].setSpeed(255);
  runDisk(BOTDISK, BACKWARD);

  uint32_t brightness_max = 0;
  int white_index = 0;
  for(int i=0; i<5; i++){
    set_target_pos(BOTDISK, mod(get_pos(BOTDISK) - 1, 5), SPACE);
    while(!is_target(BOTDISK)){ if(isStop || digitalRead(RED_BUTTON) == LOW){ motor[0].run(RELEASE); motor[1].run(RELEASE); motor[2].run(RELEASE); isStop = true; isInitialized = false; return; } }
    
    CS.setFilter(TCS230_RGB_X);
    CS.setEnable(true);    
    uint32_t brightness = CS.readSingle();
    CS.setEnable(false);    
    DUMP("Brightness is: ", brightness);
    DUMPS("\n");
    if(brightness > brightness_max){
      brightness_max = brightness;
      white_index = i;
    }

  }
  motor[BOTDISK].run(RELEASE);
  
  go(BOTDISK, BACKWARD, mod(white_index, 5), SPACE);
  go(TOPDISK, BACKWARD, 1, HOLE);

  if(!isStop){
    set_abs_pos(TOPDISK, 8);
    set_abs_pos(BOTDISK, 9);
    isInitialized = true;
  }
}


void calibrate_color(){
  if(isStop){ return; }
    
  sensorData sd;
  
  // Put this on a hole to clear old skittles
  go(BOTDISK, BACKWARD, 1, HOLE);
  
  for(int i=0; i<5; i++){
    go(TOPDISK, BACKWARD, 1, HOLE);
     CS.read();
    while(!CS.available()){ if(isStop || digitalRead(RED_BUTTON) == LOW){ motor[0].run(RELEASE); motor[1].run(RELEASE); motor[2].run(RELEASE); isStop = true; isInitialized = false; return; } };
    CS.getRaw(&sd);
    skittles[i][0] = sd.value[TCS230_RGB_R];
    skittles[i][1] = sd.value[TCS230_RGB_G];
    skittles[i][2] = sd.value[TCS230_RGB_B];
  }
}

void runDisk(int disk, uint8_t dir){
  diskDir[disk] = dir;
  motor[disk].run(dir);
}

uint8_t opposite_dir(uint8_t dir){
  return (dir == FORWARD) ? BACKWARD : FORWARD;
}


// % is the remainder operation
// this function is for modulo
// (behaves differently for negative numbers)
long mod(long a, long b)
{ return (a%b+b)%b; }

////////////////////
// Color Functions
////////////////////

int getColor(sensorData sd){
  int lowest_distance = 9999;
  int color_num;
  
  for(int i=0; i<NUM_COLORS; i++){
    
    int d = sqrt(sq(sd.value[0]-skittles[i][0])+sq(sd.value[1]-skittles[i][1])+sq(sd.value[2]-skittles[i][2]));
    Serial.print("distance for color ");
    Serial.print(i);
    Serial.print(" is ");
    Serial.println(d);
    
    if(d < lowest_distance){
      lowest_distance = d;
      color_num = i;
    }
  }
  
  Serial.print("Color index is ");
  Serial.println(color_num);

  return color_num;
}

int readColor(){
  sensorData  sd;
  colorData rgb;

  CS.read();
  while(!CS.available()){};
  CS.getRaw(&sd);
  printRaw(sd);
  
  return getColor(sd);
}

void printRGB(colorData rgb){
  Serial.println("");
  Serial.print("RGB is [");
  Serial.print(rgb.value[TCS230_RGB_R]);
  Serial.print(",");
  Serial.print(rgb.value[TCS230_RGB_G]);
  Serial.print(",");
  Serial.print(rgb.value[TCS230_RGB_B]);
  Serial.println("]");
}

void printRaw(sensorData sd){
  Serial.println("");
  Serial.print("Raw is [");
  Serial.print(sd.value[TCS230_RGB_R]);
  Serial.print(",");
  Serial.print(sd.value[TCS230_RGB_G]);
  Serial.print(",");
  Serial.print(sd.value[TCS230_RGB_B]);
  Serial.println("]");
}

