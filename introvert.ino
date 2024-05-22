/**
 * @file introvert.ino
 * @author Jonathan L. Pressler
 * @date 2024-05-21
 * @brief A simple game using an OLED display and buttons on an ESP32
 * 
 * @details This code implements a simple game where the player can move a character around the screen using buttons. 
 * The character is represented by a box, and the player can move the character up, down, left, and right. 
 * The game also includes obstacles that the player must avoid. The obstacles are represented by different shapes, 
 * such as rectangles, circles, triangles, and ellipses. The player must navigate the character around the obstacles 
 * to avoid collisions. The game logic is implemented in the loop() function, where the player's button inputs are read, 
 * and the character's position is updated based on the button presses. The moveCharacter() function handles the movement 
 * of the character on the screen and checks for collisions with obstacles. The collisionCheck() function implements collision 
 * detection logic for different types of obstacles. The drawObstacles() function draws the obstacles on the screen. 
 * The game is a simple example of how to implement a game on an Arduino using an OLED display and buttons. 
 * You can modify the code to add more features, levels, and game elements to create a more complex game.
 * 
 * @note This code is intended for educational purposes and may be used as a starting point for developing games on Arduino platforms.
 * This code utilizes the U8g2 library for OLED display functionality. Provided AS-IS for illustrative purposes only.
*/

#include <U8g2lib.h> // U8g2 OLED library
#include "intro.h" // Header file for intro animation
#include <WiFi.h> // WiFi library for ESP32
#include <ArduinoOTA.h> // Arduino OTA library for OTA updates

// Constructor for OLED display
// Uncomment this constructor if you are SPI
// U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI dRender(U8G2_R0, /* cs=*/ 5, /* dc=*/ 21, /* reset=*/ 22);

// Comment this constructor out if you are NOT using I2C
U8G2_SH1106_128X64_NONAME_F_HW_I2C dRender(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const char* ssid = "jcl-wpa2"; // WiFi SSID
const char* password = "Stealtheygrill"; // WiFi password

const int buttonPins[] = {13, 26, 14, 27};  // Array of button pins
const int numButtons = sizeof(buttonPins) / sizeof(buttonPins[0]);  // Number of buttons

int characterWidth = 10;  // Width of the character
int characterHeight = 10;  // Height of the character
int screenWidth = 128;  // Width of the screen
int screenHeight = 64;  // Height of the screen
int viewWidth = screenWidth;  // Width of the view window
int viewHeight = screenHeight;  // Height of the view window
int viewX = 0;  // X coordinate of the view window
int viewY = 0;  // Y coordinate of the view window

// Obstacle structure to represent different types of obstacles
struct Obstacle {
  int x;  // X coordinate of the obstacle
  int y;  // Y coordinate of the obstacle
  int type;  // Type of the obstacle (1 = rectangle, 2 = circle, 3 = ellipse)
  int width;  // Width of the obstacle
  int height;  // Height of the obstacle
};

// Level structure to represent different levels in the game
struct Level {
  int levelWidth;  // Width of the virtual level
  int levelHeight;  // Height of the virtual level
  int levelEndX;  // X coordinate of the level exit
  int levelEndY;  // Y coordinate of the level exit
  int exitBounding;  // Size of the exit bounding box
  Obstacle* obstacles;  // Pointer to the array of obstacles
  int numObstacles;  // Number of obstacles
  int characterX;  // Initial X coordinate of the character
  int characterY;  // Initial Y coordinate of the character
};

const int numLevels = 10;
Level levels[numLevels];
const int maxObstacles = 10;

void generateRandomLevels() {
  for (int i = 0; i < numLevels; i++) {
    int levelWidth = 150 + (i * 20);  // Increase level width with each level
    int levelHeight = 150 + (i * 20);  // Increase level height with each level
    int numObstacles = 3 + i;  // Increase number of obstacles with each level

    // Allocate memory for obstacles
    levels[i].obstacles = new Obstacle[numObstacles];

    for (int j = 0; j < numObstacles; j++) {
      int type = random(1, 4);  // Randomly choose obstacle type: 1 (rectangle), 2 (circle), 3 (ellipse)
      int width = random(10, 30);
      int height = (type == 2) ? width : random(10, 30);  // Circles have equal width and height
      int x, y;

      // Ensure obstacles are not placed at the starting or ending points
      do {
        x = random(0, levelWidth - width);
        y = random(0, levelHeight - height);
      } while ((x < 20 && y < 20) || (x > levelWidth - 40 && y > levelHeight - 40));

      levels[i].obstacles[j] = {x, y, type, width, height};
    }

    levels[i].levelWidth = levelWidth;
    levels[i].levelHeight = levelHeight;
    levels[i].levelEndX = levelWidth - 20;
    levels[i].levelEndY = levelHeight - 20;
    levels[i].exitBounding = 10;
    levels[i].numObstacles = numObstacles;
    levels[i].characterX = 10;
    levels[i].characterY = 10;
  }
}

const int debounceDelay = 50;  // Debounce time in milliseconds

unsigned long lastDebounceTime[numButtons];  // Array to store the last debounce time for each button
bool lastButtonState[numButtons];  // Array to store the last stable state for each button
bool buttonState[numButtons];  // Array to store the current stable state for each button
bool anyButtonPressed = false;  // Flag to indicate if any button is pressed
bool boundaryHit = false;  // Flag to indicate if the character hits the boundary
unsigned long boundaryHitTime = 0;  // Time when the boundary hit occurred

int timeElapsed = 0;  // Time elapsed in seconds
unsigned long gameStartTime = 0;  // Start time of the game

void setup() {
  Serial.begin(115200);
  dRender.begin();
  dRender.setFont(u8g2_font_helvB08_tr); // Set font size to 10
  WiFi.begin(ssid, password);
  for (int i = 0; i < introvert_introallArray_LEN; i++) {
    dRender.clearBuffer();
    dRender.drawXBM(0, 0, 128, 64, introvert_introallArray[i]);
    dRender.sendBuffer();
    delay(41); // Display each frame of the intro animation for 41 ms (24 frames per second)
  }
  delay(2000); // Display the last frame of the intro animation for 2 seconds

  Serial.println("Connecting to WiFi...");
  while(WiFi.status() != WL_CONNECTED) {
      delay(10); // Wait for WiFi connection before proceeding
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Seed the random number generator
  randomSeed(analogRead(0));

  // Generate random levels
  generateRandomLevels();  // Generate random levels

  // Initialize the buttons as inputs with internal pull-up resistors
  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    lastButtonState[i] = HIGH;  // Initialize lastButtonState to HIGH (unpressed)
    buttonState[i] = HIGH;  // Initialize buttonState to HIGH (unpressed)
    lastDebounceTime[i] = 0;  // Initialize lastDebounceTime to 0
  }
  ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3"); // Set OTA password to "admin"
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

}

void loop() {
  static bool initialDebug1 = true;
  static bool initialDebug2 = true;
  static bool gameStarted = false;
  static int levelNum = 0;
  Level &level = levels[levelNum];

  if (!gameStarted) {
    // Display the start screen
    int textWidth1 = dRender.getStrWidth("Press any button");
    int textWidth2 = dRender.getStrWidth("to start the game");
    int textX1 = centerTextX(textWidth1, screenWidth);
    int textX2 = centerTextX(textWidth2, screenWidth);
    dRender.clearBuffer();
    dRender.drawStr(textX1, 30, "Press any button");
    dRender.drawStr(textX2, 50, "to start the game");
    dRender.sendBuffer();
    ArduinoOTA.handle();
    if (initialDebug1) {
    Serial.println("Game not started");
    Serial.println("Arduino OTA is running and waiting for updates");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    initialDebug1 = false;
    }
  } else {
    WiFi.disconnect();
    ArduinoOTA.end();
    if (initialDebug2) {
    Serial.println("Game started");
    Serial.println("Arduino OTA has ended");
    Serial.println("WiFi has been disconnected");
    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());
    initialDebug2 = false;
    }
  }
  
  // Check the state of each button
  for (int i = 0; i < numButtons; i++) {
    checkButton(i);
  }

  // Check if any button is pressed and determine the direction
  bool up = !buttonState[0];
  bool left = !buttonState[1];
  bool down = !buttonState[2];
  bool right = !buttonState[3];

  for (int i = 0; i < numButtons; i++) {
    if (buttonState[i] == LOW) {
      anyButtonPressed = true;
      break;
    } else {
      anyButtonPressed = false;
    }
  }

  if (anyButtonPressed) {
    gameStarted = true;
    // Set current X and Y coordinates of the character based on previous state
    int x = level.characterX;
    int y = level.characterY;

    // Move the character based on the button presses
    if (up) {
      y -= 1;  // Move character up
    }
    if (down) {
      y += 1;  // Move character down
    }
    if (left) {
      x -= 1;  // Move character left
    }
    if (right) {
      x += 1;  // Move character right
    }

    // Check for collisions before updating the position
    if (!collisionCheck(level, x, y)) {
      // No collision, update character position
      level.characterX = x;
      level.characterY = y;
    }

    // Check for level completion
    if (level.characterX >= level.levelEndX && level.characterX < level.levelEndX + level.exitBounding &&
        level.characterY >= level.levelEndY && level.characterY < level.levelEndY + level.exitBounding) {
      // Level complete
      Serial.println("Level Complete");
      gameStarted = false;
      dRender.clearBuffer();
      const char* string1 = ("Level " + String(levelNum + 1) + " Complete!").c_str();
      int string1X = centerTextX(dRender.getStrWidth(string1), screenWidth);
      const char* string2 = "Time";
      int string2X = centerTextX(dRender.getStrWidth(string2), screenWidth);
      dRender.drawStr(string1X, 30, string1);
      dRender.drawStr(string2X, 50, string2);
      dRender.drawStr(string2X + 30, 50, String(timeElapsed).c_str());
      dRender.sendBuffer();
      delay(3000);

      // Move to the next level
      if (levelNum < sizeof(levels) / sizeof(levels[0]) - 1) {
        cleanupLevel(levelNum);
        levelNum++;
        level = levels[levelNum];
        gameStartTime = 0;
        gameStarted = true;
        Serial.print("Moving to level ");
        Serial.println(levelNum + 1);
        Serial.print("Free heap: ");
        Serial.println(ESP.getFreeHeap());
      }
      // Game completed
      else {
        dRender.clearBuffer();
        const char* string1 = "Game Complete!";
        int string1X = centerTextX(dRender.getStrWidth(string1), screenWidth);
        const char* string2 = "Restarting in 3 seconds";
        int string2X = centerTextX(dRender.getStrWidth(string2), screenWidth);
        dRender.drawStr(string1X, 30, string1);
        dRender.drawStr(string2X, 50, string2);
        dRender.sendBuffer();
        delay(3000);
        ESP.restart();

      }
    }

    // Adjust the view window
    if (level.characterX < viewX + viewWidth / 4) {
      viewX = level.characterX - viewWidth / 4;
    }
    if (level.characterX > viewX + 3 * viewWidth / 4) {
      viewX = level.characterX - 3 * viewWidth / 4;
    }
    if (level.characterY < viewY + viewHeight / 4) {
      viewY = level.characterY - viewHeight / 4;
    }
    if (level.characterY > viewY + 3 * viewHeight / 4) {
      viewY = level.characterY - 3 * viewHeight / 4;
    }

    // Ensure view window is within the level boundaries
    if (viewX < 0) viewX = 0;
    if (viewY < 0) viewY = 0;
    if (viewX > level.levelWidth - viewWidth) viewX = level.levelWidth - viewWidth;
    if (viewY > level.levelHeight - viewHeight) viewY = level.levelHeight - viewHeight;

    // Ensure character stays within the view boundaries
    if (level.characterX < viewX) {
      level.characterX = viewX; // Character is at the left edge of the view window
      boundaryHit = true;
    }
    if (level.characterY < viewY) {
      level.characterY = viewY; // Character is at the top edge of the view window
      boundaryHit = true;
    }
    if (level.characterX > viewX + viewWidth - characterWidth) {
      level.characterX = viewX + viewWidth - characterWidth; // Character is at the right edge of the view window
      boundaryHit = true;
    }
    if (level.characterY > viewY + viewHeight - characterHeight) {
      level.characterY = viewY + viewHeight - characterHeight; // Character is at the bottom edge of the view window
      boundaryHit = true;
    }

    // Clear the buffer and redraw the level
    dRender.clearBuffer();
    drawLevel(level);
    dRender.sendBuffer(); // Send the buffer to the display
  }
  else {
    // No button pressed, display the level and time elapsed but do not update the character position or view window
    if (gameStarted) {
      dRender.clearBuffer();
      drawTime();
      drawLevel(level);
      dRender.sendBuffer(); // Send the buffer to the display
    }
  }
}

void checkButton(int index) {
  // Read the current state of the button
  bool currentButtonState = digitalRead(buttonPins[index]);

  // Debounce logic
  if (currentButtonState != lastButtonState[index]) {
    lastDebounceTime[index] = millis();  // Reset the debounce timer
  }

  if ((millis() - lastDebounceTime[index]) > debounceDelay) {
    if (currentButtonState != buttonState[index]) {
      buttonState[index] = currentButtonState;
      if (buttonState[index] == LOW) {
        Serial.print("Button ");
        Serial.print(index + 1);
        Serial.println(" is pressed");
      }
    }
  }
  lastButtonState[index] = currentButtonState;
}

void drawLevel(Level &level) {
  // Function to draw the level within the view window
  dRender.clearBuffer();
  // Draw the character
  dRender.drawBox(level.characterX - viewX, level.characterY - viewY, characterWidth, characterHeight);

  // Draw the obstacles
  drawObstacles(level);
  
  // Draw the exit
  drawExit(level);

  // Draw the time elapsed
  drawTime();

  if (boundaryHit) { 
    Serial.println("Boundary Hit");
    // Display a message if the character hits the boundary
    if (boundaryHitTime == 0) {
      boundaryHitTime = millis(); // Record the time when the boundary hit occurred
      dRender.sendF("c", 0x0a7); // invert the display
    } else if (millis() - boundaryHitTime > 100) {
      dRender.sendF("c", 0x0a6); // revert the display
      boundaryHit = false;
      boundaryHitTime = 0;
    }
  }
}

void drawObstacles(Level &level) {
  const int box = 1;
  const int circle = 2;
  const int ellipse = 3;
  for (int i = 0; i < level.numObstacles; i++) {
    Obstacle &obstacle = level.obstacles[i];
    int x = obstacle.x - viewX;
    int y = obstacle.y - viewY;
    int type = obstacle.type;
    int width = obstacle.width;
    int height = obstacle.height;
    switch (type) {
      case box:
        dRender.drawBox(x, y, width, height);
        break;
      case circle:
        dRender.drawCircle(x + width / 2, y + height / 2, width / 2);
        break;
      case ellipse:
        dRender.drawEllipse(x + width / 2, y + height / 2, width / 2, height / 2);
        break;
    }
  }
}

bool collisionCheck(Level &level, int newX, int newY) {
  for (int i = 0; i < level.numObstacles; i++) {
    Obstacle &obstacle = level.obstacles[i];
    int x = obstacle.x;
    int y = obstacle.y;
    int type = obstacle.type;
    int width = obstacle.width;
    int height = obstacle.height;

    switch (type) {
      case 1: // Rectangle Axis-Aligned Bounding Box (AABB collision detection)
        if (newX < x + width && newX + characterWidth > x && newY < y + height && newY + characterHeight > y) {
          // Collision detected
          Serial.println("Rectangle Collision Detected");
          return true;
        }
        break;
      case 2: { // Circle (circle-to-rectangle collision detection)
        // Calculate the distance between the circle's center and the character's center
        int circleX = x + width / 2;
        int circleY = y + height / 2;
        int dx = circleX - (newX + characterWidth / 2);
        int dy = circleY - (newY + characterHeight / 2);
        int distanceSquared = dx * dx + dy * dy;

        // If the distance is less than the sum of the radii squared, a collision is detected
        int radius = width / 2;
        if (distanceSquared < (radius + characterWidth / 2) * (radius + characterHeight / 2)) {
          // Collision detected
          Serial.println("Circle Collision Detected");
          return true;
        }
        break;
      }
      case 3: { // Ellipse (ellipse-to-rectangle collision detection)
        // Check each point on the rectangle to see if it is inside the ellipse
        for (int pointX = newX; pointX <= newX + characterWidth; pointX++) {
          for (int pointY = newY; pointY <= newY + characterHeight; pointY++) {
            // Calculate the distance between the ellipse's center and this point
            int dx = x - pointX;
            int dy = y - pointY;
            float distanceSquared = (dx * dx) / ((width / 2) * (width / 2)) + (dy * dy) / ((height / 2) * (height / 2));

            // If the distance is less than 1, a collision is detected
            if (distanceSquared < 1) {
              // Collision detected
              Serial.println("Ellipse Collision Detected");
              return true;
            }
          }
        }
        break;
      }
    }
  }
  return false;
}

void drawExit(Level &level) {
  dRender.drawFrame(level.levelEndX - viewX, level.levelEndY - viewY, level.exitBounding, level.exitBounding);
}

void drawTime() {
  // Function to draw the time elapsed on the OLED display
  if (gameStartTime == 0) {
    gameStartTime = millis(); // Record the start time
  }
  timeElapsed = (millis() - gameStartTime) / 1000; // Calculate the time elapsed in seconds
  char timeStr[20]; // Buffer to hold the time string
  sprintf(timeStr, "Time: %d", timeElapsed); // Format the time string
  dRender.drawStr(viewWidth - 48, 10, timeStr); // Draw the time at the specified coordinates
}

int centerTextX(int textWidth, int screenWidth) {
  return (screenWidth - textWidth) / 2;
}

void cleanupLevel(int levelNum) {
  if (levelNum >= 0 && levelNum < numLevels) {
    delete[] levels[levelNum].obstacles;
    levels[levelNum].obstacles = nullptr;  // Set the pointer to nullptr after deleting
  }
}



  
  /* The code above is a simple example of a game where the player can move a character around the screen using buttons. The character is represented by a box, and the player can move the character up, down, left, and right. The game also includes obstacles that the player must avoid. The obstacles are represented by different shapes, such as rectangles, circles, triangles, and ellipses. The player must navigate the character around the obstacles to avoid collisions. 
  The code uses the U8g2 library to draw the character, obstacles, and game elements on an OLED display. The game logic is implemented in the loop() function, where the player's button inputs are read, and the character's position is updated based on the button presses. The moveCharacter() function handles the movement of the character on the screen and checks for collisions with obstacles. The collisionCheck() function implements collision detection logic for different types of obstacles. The drawObstacles() function draws the obstacles on the screen. 
  The game is a simple example of how to implement a game on an Arduino using an OLED display and buttons. You can modify the code to add more features, levels, and game elements to create a more complex game. 
  */
