/*
*Name: Smart chess (selfmoving chessboard)
*Author: Filip Momot
*Date:
*Description: This project uses electromagnet and xy motors to move chesspieces. 
*/

//Libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <AccelStepper.h> //ladda ner sen

//constants
const char* ssid = "IT-labbet";
const char* password = "IT-l@bbet!";
const char* serverIP = "172.17.3.232";  
const int serverPort = 10000;

const int knappar[] = {21, 22, 23, 15, 20, 18, 19, 15};
const char letters[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
const char numbers[] = {'1', '2', '3', '4', '5', '6', '7', '8'};

const int magnet = 2;
const int stepsPerSquare = 150;

bool duringMove = false;

//global variables
int joyx = 0;
int joyy = 1;
int joyButton = 2;

String drag = "";
int selectedOption = 0;
bool buttonPressed = false;
const int menuOptions = 2;
String options[] = {"Skicka", "Avbryt"};
bool letterMode = true;

int SCREEN_WIDTH = 128;
int SCREEN_HEIGHT = 64;
int OLED_RESET = -1;

//Construct objects
WiFiClient client;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AccelStepper stepper1(1, 13, 8);
AccelStepper stepper2(1, 3, 4);

//This function makes the esp connect to 
//the server to send and recive data from the python code
void connectToServer() {
  Serial.print("Ansluter till server");
  while (!client.connect(serverIP, serverPort)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nAnsluten till Python!");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  for(int i = 0; i < 8; i++){
    pinMode(knappar[i], INPUT);
  }
  pinMode(joyButton, INPUT_PULLUP);
  pinMode(magnet, OUTPUT);
  stepper1.setMaxSpeed(1000);
  stepper2.setMaxSpeed(1000);
  stepper1.setAcceleration(500);
  stepper2.setAcceleration(500);

  Wire.begin(6, 7);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println("Display not found");
    while(true);
  }
  display.clearDisplay();

  WiFi.begin(ssid, password);
  Serial.print("Ansluter till WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi ansluten!");

  connectToServer();

  goHome();
}

void loop() {
  if (!duringMove) {
    readButtons();
    handleJoystick();
  }

  drawScreen();
  delay(20);
}

// This function reads the buttons to get infomration about the move
void readButtons() {
  for(int i = 0; i < 8; i++){
    if(digitalRead(knappar[i]) == HIGH){
      if(drag.length() < 4){  //the move string cant be longer than 4 characters
        if(letterMode){
          drag += letters[i];
        } else {
          drag += numbers[i];
        }
        letterMode = !letterMode; //Switches between letters and numbers when entering moves
        delay(200);
      }
    }
  }
}

// This function control the joysticks possible movement and 
// control what happens when you press the button at diffrent locations
void handleJoystick(){
  int yValue = analogRead(joyy);
  bool pressed = digitalRead(joyButton) == LOW;

  if(yValue < 1500){
    selectedOption--;
    if(selectedOption < 0) selectedOption = menuOptions-1;
    delay(200);
  }

  if(yValue > 3000){
    selectedOption++;
    if(selectedOption >= menuOptions) selectedOption = 0;
    delay(200);
  }

  if(!buttonPressed && pressed){
    buttonPressed = true;

    if(selectedOption == 0){
      sendMove();
    }

    if(selectedOption == 1){
      drag = "";
    }
  }

  if(!pressed){
    buttonPressed = false;
  }

  if(selectedOption == 1){
  if(drag.length() > 0){
    drag.remove(drag.length() - 1);
    letterMode = !letterMode;
  }
}
}

// This function draw the menu on the screen and the move you enter 
// removes characters from the move if its too long
void drawScreen(){
  if(drag.length() > 4){ //checks if the move length is longer than 4 charackters
    drag.remove(drag.length() - 1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);
  display.println("Ditt drag:");
  display.setCursor(0,15);
  display.println(drag);  
  display.setCursor(0, 35);

  for(int i = 0; i < menuOptions; i++){

    if(i == selectedOption) display.print("> ");
    else display.print("  ");
    display.println(options[i]);
  }

  display.display();
}

// This function sends the move to stockfish 
// and at the same time recive stockfishs move
void sendMove() {
  if (drag.length() < 4) return;
  if (!client.connected()) connectToServer();

  client.println(drag);
  Serial.println("Skickade: " + drag);

  display.clearDisplay();
  display.setCursor(0, 20);
  display.println("Vantar pa svar...");
  display.display();

  unsigned long timeout = millis();
  while (!client.available()) {
    if (millis() - timeout > 8000) {
      display.clearDisplay();
      display.setCursor(0, 20);
      display.println("Timeout!");
      display.display();
      delay(1000);
      drag = "";
      return;
    }
    delay(10);
  }

  String response = client.readStringUntil('\n'); //wait for stockfish move
  response.trim();
  Serial.println("Stockfish: " + response);

  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("Stockfish:");
  display.setCursor(0, 25);
  display.println(response);
  display.display();
  delay(2000);

  String dittDrag = drag;

  drag = "";
  letterMode = true;

  duringMove = true;

  movePiece(dittDrag);
  StockfishMove(response);
}

//This function moves the electromagnet to the square 
//from which the move will be made (basead on the chess cordinates)
void moveToPosition(int x, int y) {
  int targetX = x * stepsPerSquare;
  int targetY = y * stepsPerSquare;

  stepper1.moveTo(targetX);
  stepper2.moveTo(targetY);

  while (stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0) {
    stepper1.run();
    stepper2.run();
  }
}

//This function moves my piece by turning on 
//the magnet and by moving to the right square 
void movePiece(String drag) {
  int x_cor1 = drag[0] - 'a'; //converts the move into x_coridate becuse of Unicode 'a' = 97 with means it will give a x_cordinate
  int y_cor1 = drag[1] - '1';

  int x_cor2 = drag[2] - 'a';
  int y_cor2 = drag[3] - '1';

  if(isKnightMove(x_cor1, y_cor1, x_cor2, y_cor2)) {

    Serial.println("Special knight movement");

    specialMove(drag);

    duringMove = false;
    return;
  }

  moveToPosition(x_cor1, y_cor1);

  digitalWrite(magnet, HIGH);
  delay(200);

  moveToPosition(x_cor2, y_cor2);

  digitalWrite(magnet, LOW);
  duringMove = false;
}

//This function moves the piece for stockfish by turning on the 
//electromagnet and going to the next square
void StockfishMove(String response) {
  String move = response; 

  int x1 = move[0] - 'a';
  int y1 = move[1] - '1';

  int x2 = move[2] - 'a';
  int y2 = move[3] - '1';

  moveToPosition(x1, y1);

  digitalWrite(magnet, HIGH);
  delay(200);

  moveToPosition(x2, y2);

  digitalWrite(magnet, LOW);
  
  goHome();
  duringMove = false;
}

//moves and resetes the stepmotor to (0,0)
void goHome() {
  moveToPosition(0,0);

  stepper1.setCurrentPosition(0);
  stepper2.setCurrentPosition(0);
}

//checks if the move is a knight by checking if its an L movement
bool isKnightMove(int x1, int y1, int x2, int y2) {

  int stepX = abs(x2 - x1);//distance in x directions and abs converts it from a negative number
  int stepY = abs(y2 - y1);

  return (stepX == 2 && stepY == 1) || (stepX == 1 && stepY == 2);
}

//Handles special movement for knight pieces
//including lifting and repositioning the magnet
void specialMove(String move) {
  int x1 = move[0] - 'a';
  int y1 = move[1] - '1';

  int x2 = move[2] - 'a';
  int y2 = move[3] - '1';

  moveToPosition(x1, y1);

  digitalWrite(magnet, HIGH);
  delay(200);

  moveToPosition(x1, -1);
  moveToPosition(x2, -1);

  moveToPosition(x2, y2);
  digitalWrite(magnet, LOW);

  goHome();
}