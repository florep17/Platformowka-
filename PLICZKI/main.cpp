#include "raylib.h"
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>

using namespace std;

// =========================================================
//                      STRUKTURY DANYCH
// =========================================================

struct Question {
    string text;
    string answerA;
    string answerB;
    int correctKey;
};

struct NPC {
    string name;
    string dialogue;
    int mapLocation;
    Vector2 position;
    Color color;
};

struct LevelSystem {
    int level = 1;
    float currentXP = 0.0f;
    float requiredXP = 50.0f;

    void GainXP(float amount) {
        currentXP += amount;
        while (currentXP >= requiredXP) {
            currentXP -= requiredXP;
            level++;
            requiredXP *= 1.5;
        }
    }

    void DrawHUD() {
        int screenW = GetScreenWidth();
        float barW = 220.0f;
        float barH = 20.0f;
        float margin = 20.0f;
        float x = screenW - barW - margin;
        float y = margin + 80;

        DrawRectangle(x, y, barW, barH, Fade(BLACK, 0.6f));
        float progress = currentXP / requiredXP;

        if (progress > 1.0f) {
            progress = 1.0f;
        }

        DrawRectangle(x, y, barW * progress, barH, PURPLE);
        DrawRectangleLines(x, y, barW, barH, WHITE);

        const char* lvlText = TextFormat("LVL %d", level);
        DrawText(lvlText, x - -3, y + 2, 15, WHITE);
    }
};

enum GameScreen { INTRO, MENU, GAMEPLAY, MINIGAME_FLANKI, MINIGAME_EGZAMIN, MINIGAME_DINO, KEBAB_MENU, SHOP_MENU, EXAM_MENU, DIALOGUE_MODE };
enum Location { POKOJ, PARK, KAMPUS };
enum FlankiState { AIMING, THROWING, RESULT };
enum PhoneState { PHONE_CLOSED, MENU1, PIN, ACCOUNT, APP_CRYPTO, APP_LOANS };

struct Player {
    Vector2 position;
    float baseSpeed;
    float currentSpeed;
    int level;
    Color color;

    float health;
    float hunger;
    float money;
    float debt;
    float hitboxSize;

    float speedBoostTimer;
    bool hasCheatSheet;
};

struct FlankiGame {
    FlankiState state;
    float power;
    bool powerGoingUp;
    Vector2 ballPos;
    Vector2 ballVelocity;
    bool playerWon;
    int score;
};

// Struktura dla gry Dino
struct DinoState {
    Rectangle rect;      // Pozycja gracza w minigrze
    float speedY;        // Prędkość pionowa
    bool isGrounded;     // Czy stoi na ziemi

    Rectangle cactus;    // Przeszkoda
    float cactusSpeed;   // Prędkość kaktusa

    // float scrollingBack; // USUNIĘTE - niepotrzebne przy statycznym tle

    int score;
    bool gameOver;
    bool gameStarted;    // Czy wciśnięto ENTER
};

struct Phone {
    PhoneState state;
    string correctPin;
    int selectedIcon;
    string inputPin;
    bool showError;
    int currentBet;
    string gambleInfo;
    Color infoColor;
    int loanOption;
    string loanInfo;
};

// =========================================================
//                     ZMIENNE GLOBALNE
// =========================================================

const int screenWidth = 800;
const int screenHeight = 600;

// Gracz
Player player = { { 400, 300 }, 200.0f, 200.0f, 1, WHITE, 100.0f, 100.0f, 50.0f, 0.0f, 30.0f, 0.0f, false };

// Stan gry
Location currentLocation = POKOJ;
GameScreen currentScreen = INTRO;
bool isPaused = false;

float gameTime = 8.0f;
int dayCount = 1;
float introTimer = 5.0f;

// Obiekty
FlankiGame flanki = { AIMING, 0.0f, true, {0,0}, {0,0}, false, 0 };
LevelSystem stats;
Phone phone = { PHONE_CLOSED, "0000", 0, "", false, 10, "", WHITE, 0, "" };

// Obiekt stanu gry Dino
DinoState dinoGame;

// Pomocnicze
int kebabOption = 1;
string notification = "";
float notificationTimer = 0.0f;
bool showKebabPrompt = false;
bool showFlankiPrompt = false;

int currentQuestionIndex = 0;
int activeExamTier = 0;
int activeNPCIndex = -1;

// Tekstury
Texture2D texturaIntro;
Texture2D texturaPiwa;
Texture2D texturaGracza;
Texture2D tloPokoj;
Texture2D tloFlanki;
Texture2D tloPark;
Texture2D tloKampus;
Texture2D postacTEX;
// Tekstury Dino
Texture2D tloDino;
Texture2D texturaKaktusa;

// Pytania
Question examQuestions[] = {
    {"Ile to jest 2 + 2?", "3", "4", KEY_TWO},
    {"Stolica Polski to?", "Warszawa", "Krakow", KEY_ONE},
    {"Najlepszy przyjaciel studenta?", "Sesja", "Piwo", KEY_TWO},
    {"Rok bitwy pod Grunwaldem?", "1410", "1920", KEY_ONE},
    {"Pierwiastek z 16 to?", "4", "8", KEY_ONE},
    {"Autor 'Pana Tadeusza'?", "Slowacki", "Mickiewicz", KEY_TWO},
    {"Calka z e^x to?", "e^x", "x*e^x", KEY_ONE},
    {"Jezyk tego programu to?", "Python", "C++", KEY_TWO},
    {"Kiedy sesja poprawkowa?", "We wrzesniu", "Nigdy", KEY_ONE}
};

// NPC
NPC npcs[] = {
    {"Dziekan", "Dzien dobry. Widze Pana na poprawce... (Wcisnij [1] aby zdawac)", 2, {450, 250}, BLANK},
    {"Ziomek", "Siema byczku! Potrzebujesz czegos? Mam towar... Wcisnij [S]KLEP", 1, {500, 150}, PURPLE},
    {"Starosta", "Wplaciles juz na ksero? Bo zamykam liste.", 1, {350, 250}, BLUE}
};

// =========================================================
//                  FUNKCJE POMOCNICZE
// =========================================================

void ShowNotification(string text) {
    notification = text;
    notificationTimer = 3.0f;
}

void ResetFlanki() {
    flanki.state = AIMING;
    flanki.power = 0.0f;
    flanki.powerGoingUp = true;
    flanki.ballPos = { 100, 450 };
    flanki.ballVelocity = { 0, 0 };
}

// Funkcja resetująca minigrę Dino
void ResetDino() {

    dinoGame.rect = {
        50,
        (float)screenHeight - postacTEX.height - 75,
        (float)postacTEX.width,
        (float)postacTEX.height
    };
    dinoGame.speedY = 0;
    dinoGame.isGrounded = true;

    dinoGame.cactus = { (float)screenWidth, (float)screenHeight - 135, (float)texturaKaktusa.width, (float)texturaKaktusa.height };
    dinoGame.cactusSpeed = -400.0f;
    dinoGame.score = 0;
    dinoGame.gameOver = false;
    dinoGame.gameStarted = false;
}

void SetSpawnPoint(int fromSide) {
    if (fromSide == 0) {
        player.position = { (float)screenWidth / 2, (float)screenHeight / 2 };
    }
    if (fromSide == 1) { // Z pokoju na park (lewa ścieżka)
        player.position = { 60, 320 };
    }
    if (fromSide == 2) { // Z kampusu na park
        player.position = { (float)screenWidth - 80, 320 };
    }
    if (fromSide == 3) { // Z parku do pokoju
        player.position = { 60, 350 };
    }
    if (fromSide == 4) { // Z parku na kampus
        player.position = { 50, 400 };
    }
}

void StartExam(int tier) {
    activeExamTier = tier;
    if (tier == 1) currentQuestionIndex = GetRandomValue(0, 2);
    if (tier == 2) currentQuestionIndex = GetRandomValue(3, 5);
    if (tier == 3) currentQuestionIndex = GetRandomValue(6, 8);
    currentScreen = MINIGAME_EGZAMIN;
}

void WakeUpInRoom(bool hospital) {
    currentLocation = POKOJ;
    player.position = { 500, 300 }; // Przy łóżku
    gameTime = 8.0f;
    dayCount++;

    if (hospital) {
        player.health = 100.0f;
        player.hunger = 50.0f;
        player.money -= 200.0f;
        ShowNotification("SZPITAL! Rachunek: 200 zl.");
    }
    else {
        player.health = 50.0f;
        player.hunger = 30.0f;
        player.money -= 50.0f;
        ShowNotification("Zemdlałeś z głodu! -50 zl.");
    }
}

// =========================================================
//                  LOGIKA GRY (UPDATE)
// =========================================================

void UpdateIntro(float dt) {
    introTimer -= dt;
    if (introTimer <= 0.0f) {
        currentScreen = GAMEPLAY;
    }
}

void UpdateDialogues() {
    if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_ESCAPE)) {
        currentScreen = GAMEPLAY;
        activeNPCIndex = -1;
    }

    // Logika specyficzna dla NPC
    if (activeNPCIndex != -1) {
        if (npcs[activeNPCIndex].name == "Ziomek") {
            if (IsKeyPressed(KEY_S)) currentScreen = SHOP_MENU;
        }
        if (npcs[activeNPCIndex].name == "Dziekan") {
            if (IsKeyPressed(KEY_ONE)) currentScreen = EXAM_MENU;
        }
    }
}

void UpdateSurvival(float dt) {
    // Czas
    gameTime += dt * (1.0f / 60.0f);
    if (gameTime >= 24.0f) {
        gameTime = 0.0f;
        dayCount++;
    }

    // Głód
    player.hunger -= 0.20f * dt;

    if (player.hunger <= 0.0f) {
        player.hunger = 0.0f;
        player.health -= 5.0f * dt;

        // Miganie
        if ((int)(gameTime * 10) % 2 == 0) {
            player.color = RED;
        }
        else {
            player.color = WHITE;
        }
    }
    else {
        if (player.speedBoostTimer <= 0) {
            player.color = WHITE;
        }
    }

    if (player.health <= 0.0f) {
        WakeUpInRoom(true);
    }
}

void UpdatePlayerMovement(float dt) {
    if (player.speedBoostTimer > 0.0f) {
        player.speedBoostTimer -= dt;
        player.currentSpeed = player.baseSpeed * 1.8f;
        if (player.hunger > 0) player.color = ORANGE;
    }
    else {
        player.currentSpeed = player.baseSpeed;
    }

    // Blokada ruchu gdy telefon otwarty
    if (phone.state == PHONE_CLOSED) {
        if (IsKeyDown(KEY_W)) player.position.y -= player.currentSpeed * dt;
        if (IsKeyDown(KEY_S)) player.position.y += player.currentSpeed * dt;
        if (IsKeyDown(KEY_A)) player.position.x -= player.currentSpeed * dt;
        if (IsKeyDown(KEY_D)) player.position.x += player.currentSpeed * dt;
    }
}

void UpdatePhoneLogic() {
    // Otwieranie / Zamykanie
    if (currentScreen == GAMEPLAY && IsKeyPressed(KEY_UP)) {
        if (phone.state == PHONE_CLOSED) phone.state = MENU1;
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (phone.state != PHONE_CLOSED) {
            phone.state = PHONE_CLOSED;
            phone.gambleInfo = "";
            phone.loanInfo = "";
        }
    }
    if (phone.state != PHONE_CLOSED && IsKeyPressed(KEY_Q)) {
        if (phone.state != MENU1) {
            phone.state = MENU1;
            phone.gambleInfo = "";
            phone.loanInfo = "";
        }
    }

    // PIN
    if (phone.state == PIN) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 48) && (key <= 57) && (phone.inputPin.length() < 4)) {
                phone.inputPin += (char)key;
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (phone.inputPin.length() > 0) phone.inputPin.pop_back();
        }
        if (IsKeyPressed(KEY_ENTER)) {
            if (phone.inputPin == phone.correctPin) {
                phone.state = ACCOUNT;
                phone.inputPin = "";
                phone.showError = false;
            }
            else {
                phone.showError = true;
                phone.inputPin = "";
            }
        }
    }
    // MENU
    else if (phone.state == MENU1) {
        if (IsKeyPressed(KEY_RIGHT)) {
            phone.selectedIcon++;
            if (phone.selectedIcon > 2) phone.selectedIcon = 0;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            phone.selectedIcon--;
            if (phone.selectedIcon < 0) phone.selectedIcon = 2;
        }

        if (IsKeyPressed(KEY_ENTER)) {
            if (phone.selectedIcon == 0) phone.state = PIN;
            else if (phone.selectedIcon == 1) {
                phone.state = APP_CRYPTO;
                phone.currentBet = 10;
            }
            else if (phone.selectedIcon == 2) {
                phone.state = APP_LOANS;
                phone.loanOption = 0;
                phone.loanInfo = "";
            }
        }
    }
    // KRYPTO
    else if (phone.state == APP_CRYPTO) {
        if (IsKeyPressed(KEY_UP)) phone.currentBet += 10;
        if (IsKeyPressed(KEY_DOWN)) phone.currentBet -= 10;

        if (phone.currentBet < 10) phone.currentBet = 10;

        if (IsKeyPressed(KEY_ENTER) && player.money >= phone.currentBet) {
            int chance = GetRandomValue(0, 100);
            if (chance >= 50) {
                player.money += phone.currentBet;
                phone.gambleInfo = "Wygrana!";
                phone.infoColor = GREEN;
                stats.GainXP(5.0f);
            }
            else {
                player.money -= phone.currentBet;
                phone.gambleInfo = "Przegrana...";
                phone.infoColor = RED;
            }
        }
    }
    // CHWILOWKI
    else if (phone.state == APP_LOANS) {
        if (IsKeyPressed(KEY_DOWN)) phone.loanOption++;
        if (IsKeyPressed(KEY_UP)) phone.loanOption--;
        if (phone.loanOption > 3) phone.loanOption = 0;
        if (phone.loanOption < 0) phone.loanOption = 3;

        if (IsKeyPressed(KEY_ENTER)) {
            if (phone.loanOption == 0) { player.money += 100.0f; player.debt += 120.0f; phone.loanInfo = "Wzieles 100 zl!"; }
            else if (phone.loanOption == 1) { player.money += 500.0f; player.debt += 700.0f; phone.loanInfo = "Wzieles 500 zl!"; }
            else if (phone.loanOption == 2) { player.money += 1000.0f; player.debt += 1500.0f; phone.loanInfo = "Wzieles 1000 zl!"; }
            else if (phone.loanOption == 3) {
                if (player.debt > 0 && player.money > 0) {
                    if (player.money >= player.debt) {
                        player.money -= player.debt;
                        player.debt = 0;
                        phone.loanInfo = "Splacono!";
                    }
                    else {
                        player.debt -= player.money;
                        player.money = 0;
                        phone.loanInfo = "Splaciles czesc.";
                    }
                }
                else {
                    phone.loanInfo = "Brak kasy/dlugu.";
                }
            }
        }
    }
}

void UpdateCollisionsAndWorld() {
    // Granice mapy
    if (currentLocation == POKOJ) {
        if (player.position.x < 0) player.position.x = 0;
        if (player.position.x > 710) player.position.x = 710;
        if (player.position.y < 250) player.position.y = 250;
        if (player.position.y > 560) player.position.y = 560;
    }
    else {
        // Granice dla reszty map
        if (player.position.x < 0) player.position.x = 0;
        if (player.position.y < 0) player.position.y = 0;
        if (player.position.x > screenWidth - player.hitboxSize) player.position.x = screenWidth - player.hitboxSize;
        if (player.position.y > screenHeight - player.hitboxSize) player.position.y = screenHeight - player.hitboxSize;
    }

    Rectangle playerRect = { player.position.x, player.position.y, player.hitboxSize, player.hitboxSize };

    // --- POKÓJ ---
    if (currentLocation == POKOJ) {
        // Łóżko (prawa strona)
        Rectangle bedRect = { 450, 200, 200, 150 };
        if (CheckCollisionRecs(playerRect, bedRect) && IsKeyPressed(KEY_E)) {
            player.health += 50.0f; if (player.health > 100) player.health = 100;
            player.hunger -= 20.0f; gameTime = 8.0f; dayCount++;
            ShowNotification("Wyspales sie! Dzien ++");
        }

        // Drzwi do Parku (lewa strona)
        Rectangle doorToPark = { 0, 300, 30, 200 };
        if (CheckCollisionRecs(playerRect, doorToPark)) {
            currentLocation = PARK;
            SetSpawnPoint(1);
        }
    }
    // --- PARK ---
    else if (currentLocation == PARK) {
        Rectangle doorToRoom = { 0, 280, 20, 100 };
        Rectangle doorToCampus = { (float)screenWidth - 20, 280, 20, 100 };
        Rectangle kebabRect = { 100, 100, 150, 100 };
        Rectangle flankiZone = { 100, 450, 150, 100 };

        if (CheckCollisionRecs(playerRect, kebabRect)) {
            showKebabPrompt = true;
            if (IsKeyPressed(KEY_E)) {
                currentScreen = KEBAB_MENU;
                kebabOption = 1;
            }
        }

        if (CheckCollisionRecs(playerRect, doorToRoom)) {
            currentLocation = POKOJ;
            SetSpawnPoint(3);
        }
        else if (CheckCollisionRecs(playerRect, doorToCampus)) {
            currentLocation = KAMPUS;
            SetSpawnPoint(4);
        }

        if (CheckCollisionRecs(playerRect, flankiZone)) {
            showFlankiPrompt = true;
            if (IsKeyPressed(KEY_E)) {
                currentScreen = MINIGAME_FLANKI;
                ResetFlanki();
            }
        }

        // WYZWALACZ GRY DINO
        // Jeśli gracz pójdzie na sam dół mapy (Park jest otwarty)
        if (player.position.y >= screenHeight - player.hitboxSize - 10) {
            currentScreen = MINIGAME_DINO;
            ResetDino();
            // Cofamy gracza trochę do góry, żeby po powrocie nie wpadł znowu w trigger
            player.position.y -= 50;
        }
    }
    // --- KAMPUS ---
    else if (currentLocation == KAMPUS) {
        // Drzwi do Parku (górny lewy róg)
        Rectangle doorToPark = { 190, 220, 70, 20 };
        if (CheckCollisionRecs(playerRect, doorToPark)) {
            currentLocation = PARK;
            SetSpawnPoint(2);
        }
    }

    // --- INTERAKCJE Z NPC ---
    for (int i = 0; i < 3; i++) {
        if (npcs[i].mapLocation == currentLocation) {
            float triggerSize = (npcs[i].name == "Dziekan") ? 150.0f : 80.0f;
            Rectangle npcRect = { npcs[i].position.x, npcs[i].position.y, 60, triggerSize };

            if (CheckCollisionRecs(playerRect, npcRect)) {
                if (IsKeyPressed(KEY_E)) {
                    currentScreen = DIALOGUE_MODE;
                    activeNPCIndex = i;
                }
            }
        }
    }
}

void UpdateMinigamesLogic(float dt) {
    // Flanki
    if (currentScreen == MINIGAME_FLANKI) {
        if (IsKeyPressed(KEY_Q)) currentScreen = GAMEPLAY;

        if (flanki.state == AIMING) {
            float speed = 1.5f * dt;
            if (flanki.powerGoingUp) flanki.power += speed;
            else flanki.power -= speed;

            if (flanki.power >= 1.0f) flanki.powerGoingUp = false;
            if (flanki.power <= 0.0f) flanki.powerGoingUp = true;

            if (IsKeyPressed(KEY_SPACE)) {
                flanki.state = THROWING;
                flanki.ballVelocity = { 400.0f + (flanki.power * 200.0f), -300.0f - (flanki.power * 300.0f) };
            }
        }
        else if (flanki.state == THROWING) {
            flanki.ballPos.x += flanki.ballVelocity.x * dt;
            flanki.ballPos.y += flanki.ballVelocity.y * dt;
            flanki.ballVelocity.y += 980.0f * dt;

            if (CheckCollisionCircleRec({ flanki.ballPos.x + 10, flanki.ballPos.y + 10 }, 10, { 700, 400, 60, 100 })) {
                flanki.playerWon = true;
                flanki.score++;
                stats.GainXP(10.0f);
                flanki.state = RESULT;
            }
            if (flanki.ballPos.y > 500) {
                flanki.playerWon = false;
                flanki.state = RESULT;
            }
        }
        else if (flanki.state == RESULT) {
            if (IsKeyPressed(KEY_SPACE)) ResetFlanki();
        }
    }

    // Egzamin
    if (currentScreen == MINIGAME_EGZAMIN) {
        if (player.hasCheatSheet && IsKeyPressed(KEY_SPACE)) {
            float reward = (activeExamTier == 1) ? 50.0f : (activeExamTier == 2 ? 150.0f : 500.0f);
            player.money += reward;
            stats.GainXP(activeExamTier * 20.0f);
            player.hasCheatSheet = false;
            currentScreen = GAMEPLAY;
            return;
        }
        if (IsKeyPressed(examQuestions[currentQuestionIndex].correctKey)) {
            float reward = (activeExamTier == 1) ? 50.0f : (activeExamTier == 2 ? 150.0f : 500.0f);
            player.money += reward;
            stats.GainXP(activeExamTier * 20.0f);
            currentScreen = GAMEPLAY;
        }
        else if (GetKeyPressed() != 0) {
            currentScreen = GAMEPLAY;
        }
    }

    // Logika DINO
    if (currentScreen == MINIGAME_DINO) {
        // Wyjście z minigry
        if (IsKeyPressed(KEY_Q)) {
            currentScreen = GAMEPLAY;
            return;
        }

        if (!dinoGame.gameStarted) {
            if (IsKeyPressed(KEY_ENTER)) dinoGame.gameStarted = true;
        }
        else if (!dinoGame.gameOver) {
          
            float gravity = 800.0f;
            float jumpForce = -450.0f;

           

            // Skok
            if (IsKeyPressed(KEY_SPACE) && dinoGame.isGrounded) {
                dinoGame.speedY = jumpForce;
                dinoGame.isGrounded = false;
            }

            // Fizyka
            dinoGame.speedY += gravity * dt;
            dinoGame.rect.y += dinoGame.speedY * dt;

            // Kolizja z ziemią (margines 75 od dołu)
            float groundLevel = screenHeight - dinoGame.rect.height - 75;
            if (dinoGame.rect.y >= groundLevel) {
                dinoGame.rect.y = groundLevel;
                dinoGame.speedY = 0;
                dinoGame.isGrounded = true;
            }

            // Ruch kaktusa
            dinoGame.cactus.x += dinoGame.cactusSpeed * dt;

            // Reset kaktusa
            if (dinoGame.cactus.x < -dinoGame.cactus.width) {
                dinoGame.cactus.x = screenWidth + (float)GetRandomValue(0, 300);
                dinoGame.score++;
                dinoGame.cactusSpeed -= 20; // Przyspieszenie
                stats.GainXP(2.0f); // XP za każdą przeszkodę!
            }

            // Kolizja
            if (CheckCollisionRecs(dinoGame.rect, dinoGame.cactus)) {
                dinoGame.gameOver = true;
            }
        }
        else {
            // Game Over - Restart
            if (IsKeyPressed(KEY_R)) {
                ResetDino();
                dinoGame.gameStarted = true;
            }
        }
    }

    // Menus
    if (currentScreen == KEBAB_MENU) {
        if (IsKeyPressed(KEY_Q)) currentScreen = GAMEPLAY;
        if (IsKeyPressed(KEY_DOWN)) kebabOption++;
        if (IsKeyPressed(KEY_UP)) kebabOption--;
        if (kebabOption > 2) kebabOption = 0;
        if (kebabOption < 0) kebabOption = 2;

        if (IsKeyPressed(KEY_ENTER)) {
            if (kebabOption == 0 && player.money >= 8.0f) {
                player.money -= 8.0f; player.hunger += 20.0f; currentScreen = GAMEPLAY;
            }
            else if (kebabOption == 1 && player.money >= 18.0f) {
                player.money -= 18.0f; player.hunger += 50.0f; currentScreen = GAMEPLAY;
            }
            else if (kebabOption == 2 && player.money >= 30.0f) {
                player.money -= 30.0f; player.hunger = 100.0f; player.health += 30.0f; currentScreen = GAMEPLAY;
            }
            if (player.hunger > 100.0f) player.hunger = 100.0f;
            if (player.health > 100.0f) player.health = 100.0f;
        }
    }

    if (currentScreen == SHOP_MENU) {
        if (IsKeyPressed(KEY_Q)) currentScreen = GAMEPLAY;
        if (IsKeyPressed(KEY_ONE) && player.money >= 20.0f) {
            player.money -= 20.0f; player.speedBoostTimer = 20.0f;
        }
        if (IsKeyPressed(KEY_TWO) && player.money >= 100.0f && !player.hasCheatSheet) {
            player.money -= 100.0f; player.hasCheatSheet = true;
        }
    }

    if (currentScreen == EXAM_MENU) {
        if (IsKeyPressed(KEY_Q)) currentScreen = GAMEPLAY;
        if (IsKeyPressed(KEY_ONE)) StartExam(1);
        if (IsKeyPressed(KEY_TWO) && stats.level >= 2) StartExam(2);
        if (IsKeyPressed(KEY_THREE) && stats.level >= 4) StartExam(3);
    }
}

// Główna funkcja UPDATE
void UpdateGame(float dt) {
    // Intro
    if (currentScreen == INTRO) {
        UpdateIntro(dt);
        return;
    }

    if (IsKeyPressed(KEY_TAB)) isPaused = !isPaused;
    if (isPaused) return;

    if (notificationTimer > 0) notificationTimer -= dt;
    showKebabPrompt = false;
    showFlankiPrompt = false;

    if (currentScreen == DIALOGUE_MODE) {
        UpdateDialogues();
        return;
    }

    if (currentScreen == GAMEPLAY) {
        UpdateSurvival(dt);
        UpdatePlayerMovement(dt);
        UpdatePhoneLogic();
        UpdateCollisionsAndWorld();
    }
    else {
        UpdateMinigamesLogic(dt);
    }
}

// =========================================================
//                  RYSOWANIE GRY (DRAW)
// =========================================================

void DrawMinigameScreens() {
    if (currentScreen == MINIGAME_FLANKI) {
        DrawTexturePro(tloFlanki, { 0,0,(float)tloFlanki.width, (float)tloFlanki.height }, { 0,0,(float)screenWidth, (float)screenHeight }, { 0,0 }, 0.0f, WHITE);
        DrawText("FLANKI", 20, 20, 20, DARKGRAY);
        DrawText(TextFormat("Wynik: %d", flanki.score), 20, 50, 20, BLACK);
        DrawText("[Q] - Powrot", 700, 20, 15, BLACK);

        Rectangle zrodloPiwo = { 0.0f, 0.0f, (float)texturaPiwa.width, (float)texturaPiwa.height };
        Rectangle celPiwo = { 700.0f, 400.0f, 60.0f, 100.0f };
        DrawTexturePro(texturaPiwa, zrodloPiwo, celPiwo, { 0,0 }, 0.0f, WHITE);

        Rectangle zrodloGracz = { 0.0f, 0.0f, (float)texturaGracza.width, (float)texturaGracza.height };
        Rectangle celGraczFlanki = { 50.0f, 380.0f, 120.0f, 120.0f };
        DrawTexturePro(texturaGracza, zrodloGracz, celGraczFlanki, { 0,0 }, 0.0f, WHITE);

        DrawCircleV(flanki.ballPos, 10, DARKGRAY);

        if (flanki.state == AIMING) {
            DrawRectangle(50, 100, 30, 200, LIGHTGRAY);
            DrawRectangle(50, 150, 30, 50, GREEN);
            int barHeight = (int)(flanki.power * 200);
            DrawRectangle(50, 300 - barHeight, 30, 5, RED);
            DrawText("SPACJA: Rzuc!", 100, 200, 20, BLACK);
        }
        else if (flanki.state == RESULT) {
            if (flanki.playerWon) DrawText("TRAFIONY!", 300, 200, 40, GOLD);
            else DrawText("PUDLO!", 300, 200, 40, MAROON);
        }
        stats.DrawHUD();
    }
    else if (currentScreen == MINIGAME_EGZAMIN) {
        ClearBackground(LIGHTGRAY);
        DrawText("EGZAMIN W TOKU...", 50, 50, 30, DARKBLUE);
        DrawText(examQuestions[currentQuestionIndex].text.c_str(), 100, 200, 30, BLACK);
        DrawText(TextFormat("1. %s", examQuestions[currentQuestionIndex].answerA.c_str()), 100, 300, 25, DARKGRAY);
        DrawText(TextFormat("2. %s", examQuestions[currentQuestionIndex].answerB.c_str()), 100, 350, 25, DARKGRAY);
        DrawText("Wybierz 1 lub 2 na klawiaturze", 100, 500, 20, RED);

        if (player.hasCheatSheet) {
            DrawText("SCIAGA DOSTEPNA [SPACJA]", 100, 550, 20, GOLD);
        }
    }
    // Rysowanie DINO
    else if (currentScreen == MINIGAME_DINO) {
        ClearBackground(RAYWHITE);
        DrawTexture(tloDino, 0, screenHeight - tloDino.height, WHITE);
        DrawLine(0, screenHeight - 75, screenWidth, screenHeight - 75, BLACK);

        // RYSOWANIE NOWEJ TEKSTURY
        DrawTexture(postacTEX, (int)dinoGame.rect.x, (int)dinoGame.rect.y, WHITE);

        DrawTexture(texturaKaktusa, (int)dinoGame.cactus.x, (int)dinoGame.cactus.y, WHITE);

        // UI
        DrawText(TextFormat("Wynik: %i", dinoGame.score), 20, 20, 20, BLACK);
        DrawText("[Q] - Powrot", 700, 20, 15, BLACK);

        if (!dinoGame.gameStarted) {
            DrawText("AGH RUN", screenWidth / 2 - 100, screenHeight / 2 - 60, 50, BLACK);
            DrawText("Nacisnij ENTER, aby zaczac", screenWidth / 2 - 160, screenHeight / 2 + 10, 25, BLACK);
            DrawText("Przeskakuj puszki aby otrzymac doswiadczenie!", screenWidth / 2 - 220, screenHeight / 2 - 150, 25, BLACK);
        }
        else if (dinoGame.gameOver) {
            DrawText("GAME OVER!", screenWidth / 2 - 100, screenHeight / 2 - 200, 40, BLACK);
            DrawText("Nacisnij 'R', aby sprobowac ponownie", screenWidth / 2 - 150, screenHeight / 2 - 150, 20, BLACK);
        }
        stats.DrawHUD();
    }
}


void DrawWorldAndPlayer() {
    if (currentLocation == POKOJ) {
        DrawTexturePro(tloPokoj, { 0,0,(float)tloPokoj.width, (float)tloPokoj.height }, { 0,0,(float)screenWidth, (float)screenHeight }, { 0,0 }, 0.0f, WHITE);
        DrawText("POKOJ STUDENTA", 20, 20, 20, DARKGRAY);
        DrawText("LOZKO [E]", 500, 180, 20, WHITE);
    }
    else if (currentLocation == PARK) {
        DrawTexturePro(tloPark, { 0,0,(float)tloPark.width, (float)tloPark.height }, { 0,0,(float)screenWidth, (float)screenHeight }, { 0,0 }, 0.0f, WHITE);
        DrawText("PARK", 20, 20, 20, WHITE);

        // Ciemność tylko w parku
        if (gameTime >= 20.0f || gameTime < 6.0f) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
        }
        else if (gameTime >= 18.0f) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(ORANGE, 0.2f));
        }

        if (showKebabPrompt) DrawText("Kebab [E]", 120, 80, 20, WHITE);
        if (showFlankiPrompt) DrawText("Flanki [E]", 120, 430, 20, WHITE);

        // Info o bieganiu na dole
        DrawText("IDZ POBIEGAC (DOL EKRANU)", screenWidth / 2 - 100, screenHeight - 30, 20, WHITE);
    }
    else if (currentLocation == KAMPUS) {
        DrawTexturePro(tloKampus, { 0,0,(float)tloKampus.width, (float)tloKampus.height }, { 0,0,(float)screenWidth, (float)screenHeight }, { 0,0 }, 0.0f, WHITE);
        DrawText("KAMPUS", 20, 20, 20, DARKBLUE);
        DrawText("EGZAMIN [E]", 450, 230, 20, BLACK);
        DrawText("WYJSCIE", 195, 200, 10, DARKGRAY);
    }

    // Gracz
    if (currentScreen == GAMEPLAY) {
        float drawSize = 90.0f;
        if (currentLocation == POKOJ || currentLocation == KAMPUS) drawSize = 240.0f;

        float offsetXY = (drawSize - player.hitboxSize) / 2.0f;
        Rectangle zrodloStudent = { 0.0f, 0.0f, (float)texturaGracza.width, (float)texturaGracza.height };
        Rectangle celStudent = { player.position.x - offsetXY, player.position.y - offsetXY, drawSize, drawSize };
        DrawTexturePro(texturaGracza, zrodloStudent, celStudent, { 0,0 }, 0.0f, player.color);
    }

    // NPC
    for (int i = 0; i < 3; i++) {
        if (npcs[i].mapLocation == currentLocation) {
            if (npcs[i].color.a != 0) {
                DrawTexturePro(texturaGracza, { 0,0,(float)texturaGracza.width,(float)texturaGracza.height }, { npcs[i].position.x, npcs[i].position.y, 100, 100 }, { 0,0 }, 0.0f, npcs[i].color);
                DrawText(npcs[i].name.c_str(), npcs[i].position.x, npcs[i].position.y - 20, 10, WHITE);
            }
        }
    }
}

void DrawInterface() { // HUD
    stats.DrawHUD();
    DrawText(TextFormat("DZIEN %d | %02d:00", dayCount, (int)gameTime), screenWidth - 180, 20, 20, WHITE);

    float barW = 220.0f; float barH = 20.0f;
    float x = screenWidth - barW - 20; float y = 50.0f;

    // Pasek Życia
    DrawRectangle(x, y, barW, barH, Fade(BLACK, 0.6f));
    DrawRectangle(x, y, barW * (player.health / 100.0f), barH, RED);
    DrawRectangleLines(x, y, barW, barH, WHITE);
    DrawText("ZYCIE", x + 5, y + 2, 10, WHITE);

    y += 25;
    // Pasek Głodu
    DrawRectangle(x, y, barW, barH, Fade(BLACK, 0.6f));
    DrawRectangle(x, y, barW * (player.hunger / 100.0f), barH, (player.hunger < 20.0f) ? ORANGE : GREEN);
    DrawRectangleLines(x, y, barW, barH, WHITE);
    DrawText("GLOD", x + 5, y + 2, 10, WHITE);

    if (notificationTimer > 0) {
        DrawText(notification.c_str(), screenWidth / 2 - MeasureText(notification.c_str(), 30) / 2, 100, 30, RED);
    }
}

void DrawPhone() {
    if (phone.state != PHONE_CLOSED) {
        int px = 550, py = 150;
        int pw = 240, ph = 420;


        DrawRectangleRounded({ (float)px, (float)py, (float)pw, (float)ph }, 0.1f, 10, DARKGRAY);
        DrawRectangleRoundedLines({ (float)px, (float)py, (float)pw, (float)ph }, 0.1f, 10, BLACK);

        DrawRectangle(px + 10, py + 40, pw - 20, ph - 60, RAYWHITE);
        DrawCircle(px + pw / 2, py + 20, 5, BLACK);
        DrawText("100%", px + pw - 40, py + 15, 10, LIGHTGRAY);

        if (phone.state == MENU1) {
            DrawRectangle(px + 10, py + 40, pw - 20, ph - 60, LIGHTGRAY);
            DrawRectangle(px + 30, py + 80, 50, 50, WHITE); DrawText("BANK", px + 35, py + 100, 10, BLACK);
            if (phone.selectedIcon == 0) DrawRectangleLines(px + 30, py + 80, 50, 50, RED);

            DrawRectangle(px + 95, py + 80, 50, 50, BLACK); DrawText("CRYPTO", px + 97, py + 100, 10, GREEN);
            if (phone.selectedIcon == 1) DrawRectangleLines(px + 95, py + 80, 50, 50, RED);

            DrawRectangle(px + 160, py + 80, 50, 50, BLUE); DrawText("LOAN", px + 168, py + 100, 10, WHITE);
            if (phone.selectedIcon == 2) DrawRectangleLines(px + 160, py + 80, 50, 50, RED);
        }
        else if (phone.state == PIN) {
            DrawText("WPROWADZ PIN:", px + 50, py + 100, 15, BLACK);
            DrawText(phone.inputPin.c_str(), px + 80, py + 140, 30, BLACK);
            if (phone.showError) DrawText("BLEDNY PIN!", px + 60, py + 180, 15, RED);
        }
        else if (phone.state == ACCOUNT) {
            DrawText("SALDO KONTA:", px + 20, py + 80, 10, DARKGRAY);
            DrawText(TextFormat("%.2f zl", player.money), px + 20, py + 100, 30, GREEN);
            DrawText("AKTUALNY DLUG:", px + 20, py + 150, 10, DARKGRAY);
            DrawText(TextFormat("%.2f", player.debt), px + 20, py + 170, 30, RED);
            DrawText("[Q] Wroc", px + 90, py + 380, 10, GRAY);
        }
        else if (phone.state == APP_CRYPTO) {
            DrawRectangle(px + 10, py + 40, pw - 20, ph - 60, BLACK);
            DrawText("CRYPTO MARKET", px + 50, py + 60, 15, PURPLE);
            DrawText(TextFormat("Twoje: %.2f zl", player.money), px + 20, py + 100, 10, GREEN);
            DrawText(TextFormat("Stawka: %d zl", phone.currentBet), px + 60, py + 200, 20, WHITE);
            DrawText("GORA/DOL [ARROWS]", px + 40, py + 230, 10, DARKGRAY);
            DrawText("INWESTUJ [ENTER]", px + 50, py + 280, 15, GOLD);
            DrawText(phone.gambleInfo.c_str(), px + 50, py + 330, 15, phone.infoColor);
        }
        else if (phone.state == APP_LOANS) {
            DrawText("SZYBKA KASA", px + 60, py + 60, 20, BLUE);
            DrawText(phone.loanInfo.c_str(), px + 40, py + 90, 10, DARKBLUE);

            Color c0 = BLACK, c1 = BLACK, c2 = BLACK, c3 = BLACK;
            if (phone.loanOption == 0) c0 = RED; if (phone.loanOption == 1) c1 = RED; if (phone.loanOption == 2) c2 = RED; if (phone.loanOption == 3) c3 = RED;

            int offY = py + 130;
            DrawText("1. Mala (100)", px + 40, offY, 15, c0);
            DrawText("2. Srednia (500)", px + 40, offY + 30, 15, c1);
            DrawText("3. Duza (1000)", px + 40, offY + 60, 15, c2);
            DrawText("4. SPLAC DLUG", px + 40, offY + 120, 15, c3);

            if (phone.loanOption >= 0 && phone.loanOption <= 2) {
                float prowizja = 0;
                if (phone.loanOption == 0) prowizja = 20; if (phone.loanOption == 1) prowizja = 200; if (phone.loanOption == 2) prowizja = 500;
                DrawText(TextFormat("Prowizja: %.0f zl", prowizja), px + 40, offY + 90, 10, RED);
            }
        }
    }
}

void DrawPopups() {
    if (currentScreen == DIALOGUE_MODE && activeNPCIndex != -1) {
        DrawRectangle(0, 400, screenWidth, 200, Fade(BLACK, 0.8f));
        DrawRectangleLines(0, 400, screenWidth, 200, WHITE);
        DrawText(npcs[activeNPCIndex].name.c_str(), 50, 420, 30, (npcs[activeNPCIndex].color.a == 0 ? BLACK : npcs[activeNPCIndex].color));
        DrawText(npcs[activeNPCIndex].dialogue.c_str(), 50, 470, 20, WHITE);
        DrawText("[Q] Zakoncz", 600, 560, 15, GRAY);
    }
    if (currentScreen == KEBAB_MENU) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
        DrawRectangle(200, 150, 400, 300, RAYWHITE);
        DrawRectangleLines(200, 150, 400, 300, BLACK);
        DrawText("ALIBABA KEBAB", 330, 165, 20, BLACK);
        Color c1 = (kebabOption == 0) ? RED : BLACK; Color c2 = (kebabOption == 1) ? RED : BLACK; Color c3 = (kebabOption == 2) ? RED : BLACK;
        DrawText("1. Frytki (8zl, Glod+)", 240, 260, 20, c1);
        DrawText("2. Rollo (18zl, Glod++)", 240, 300, 20, c2);
        DrawText("3. Kubel XXL (30zl, Glod MAX, HP+)", 240, 340, 20, c3);
        DrawText(TextFormat("Kasa: %.2f", player.money), 240, 400, 20, DARKGREEN);
    }
    if (currentScreen == SHOP_MENU) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
        DrawRectangle(100, 100, 600, 400, DARKGRAY);
        DrawRectangleLines(100, 100, 600, 400, WHITE);
        DrawText("SKLEP ZIOMKA", 250, 120, 30, WHITE);
        DrawText("1. Energetyk (20zl)", 150, 200, 20, GREEN);
        DrawText("2. Sciaga (100zl)", 150, 250, 20, YELLOW);
        DrawText(TextFormat("Kasa: %.2f", player.money), 150, 350, 20, WHITE);
    }
    if (currentScreen == EXAM_MENU) {
        DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
        DrawRectangle(100, 100, 600, 400, RAYWHITE); DrawRectangleLines(100, 100, 600, 400, BLACK);
        DrawText("WYBIERZ EGZAMIN [1-3]", 200, 120, 30, DARKBLUE);
        DrawText("1. Latwy (50zl)", 150, 200, 20, DARKGREEN);
        DrawText(stats.level >= 2 ? "2. Sredni (150zl)" : "2. Sredni (Wymaga LVL 2)", 150, 250, 20, stats.level >= 2 ? DARKGREEN : GRAY);
        DrawText(stats.level >= 4 ? "3. Trudny (500zl)" : "3. Trudny (Wymaga LVL 4)", 150, 300, 20, stats.level >= 4 ? DARKGREEN : GRAY);
    }
}

// Główna funkcja DRAW
void DrawGame() {
    BeginDrawing();
    ClearBackground(BLACK);

    if (currentScreen == INTRO) {
        DrawTexturePro(texturaIntro, { 0.0f, 0.0f, (float)texturaIntro.width, (float)texturaIntro.height }, { 0.0f, 0.0f, (float)screenWidth, (float)screenHeight }, { 0.0f, 0.0f }, 0.0f, WHITE);
    }
    else if (currentScreen == MINIGAME_FLANKI || currentScreen == MINIGAME_EGZAMIN || currentScreen == MINIGAME_DINO) {
        DrawMinigameScreens();
    }
    else {
        DrawWorldAndPlayer();
        DrawInterface();
        DrawPopups();
        DrawPhone();

        if (isPaused) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
            DrawText("PAUZA", 350, 280, 40, WHITE);
        }
    }
    EndDrawing();
}

int main() {
    InitWindow(screenWidth, screenHeight, "Student RPG");
    SetTargetFPS(60);

    texturaIntro = LoadTexture("assets/jeden.png");
    texturaPiwa = LoadTexture("assets/piwo.png");
    texturaGracza = LoadTexture("assets/postac.png");
    tloPokoj = LoadTexture("assets/akademik.png");
    tloFlanki = LoadTexture("assets/tloflanki.png");
    tloPark = LoadTexture("assets/park.png");
    tloKampus = LoadTexture("assets/kampus.png");
    postacTEX = LoadTexture("assets/postac2.png");
    // Tekstury Dino
    tloDino = LoadTexture("assets/tor.png");
    texturaKaktusa = LoadTexture("assets/przeszkoda.png");

    while (!WindowShouldClose()) {
        UpdateGame(GetFrameTime());
        DrawGame();
    }

    UnloadTexture(texturaIntro);
    UnloadTexture(texturaPiwa);
    UnloadTexture(texturaGracza);
    UnloadTexture(tloPokoj);
    UnloadTexture(tloFlanki);
    UnloadTexture(tloPark);
    UnloadTexture(tloKampus);


    UnloadTexture(tloDino);
    UnloadTexture(texturaKaktusa);
    UnloadTexture(postacTEX);

    CloseWindow();
    return 0;
}
