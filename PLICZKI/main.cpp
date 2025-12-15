#include "raylib.h"
#include <string>
#include <vector>
#include <cmath> 
#include <cstdio> 

using namespace std;

// --- DANE GRY ---

struct Question {
    string text;       
    string answerA;    
    string answerB;    
    int correctKey;    
};

// Pytania podzielone na poziomy trudności
Question examQuestions[] = {
    // POZIOM 1 (ŁATWE)
    {"Ile to jest 2 + 2?", "3", "4", KEY_TWO},
    {"Stolica Polski to?", "Warszawa", "Krakow", KEY_ONE},
    {"Najlepszy przyjaciel studenta?", "Sesja", "Piwo", KEY_TWO},
    
    // POZIOM 2 (ŚREDNIE)
    {"Rok bitwy pod Grunwaldem?", "1410", "1920", KEY_ONE},
    {"Pierwiastek z 16 to?", "4", "8", KEY_ONE},
    {"Autor 'Pana Tadeusza'?", "Slowacki", "Mickiewicz", KEY_TWO},

    // POZIOM 3 (TRUDNE)
    {"Calka z e^x to?", "e^x", "x*e^x", KEY_ONE},
    {"Jezyk tego programu to?", "Python", "C++", KEY_TWO},
    {"Kiedy sesja poprawkowa?", "We wrzesniu", "Nigdy", KEY_ONE}
};

int currentQuestionIndex = 0; 
int activeExamTier = 0; 

// --- NPC ---
struct NPC {
    string name;
    string dialogue;
    int mapLocation; // rzutowane na enum Location
    Vector2 position;
    Color color;
};

NPC npcs[] = {
    {"Dziekan", "Dzien dobry. Widze Pana na poprawce we wrzesniu...", 2, {200, 400}, RED}, // KAMPUS
    {"Ziomek", "Ej mordo, pozycz 2 zlote na piwo, oddam jutro!", 1, {500, 150}, PURPLE},   // PARK
    {"Starosta", "Wplaciles juz na ksero? Bo zamykam liste.", 1, {300, 300}, BLUE}         // PARK
};
int activeNPCIndex = -1;

// --- SYSTEM LEVELOWANIA ---
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
        
        // Przesuwamy niżej, żeby nie zasłaniało paska życia/głodu
        float y = margin + 80; 

        DrawRectangle(x, y, barW, barH, Fade(BLACK, 0.6f));
        float progress = currentXP / requiredXP;
        if (progress > 1.0f) {
            progress = 1.0f; 
        }
        
        DrawRectangle(x, y, barW * progress, barH, PURPLE);
        DrawRectangleLines(x, y, barW, barH, WHITE);
        
        const char* lvlText = TextFormat("LVL %d", level);
        DrawText(lvlText, x - 60, y + 2, 20, WHITE);
    }
};

// --- ENUMY ---
enum GameScreen { MENU, GAMEPLAY, MINIGAME_FLANKI, MINIGAME_EGZAMIN, KEBAB_MENU, SHOP_MENU, EXAM_MENU, DIALOGUE_MODE };
enum Location { POKOJ, PARK, KAMPUS };
enum FlankiState { AIMING, THROWING, RESULT };
enum PhoneState { PHONE_CLOSED, MENU1, PIN, ACCOUNT, APP_CRYPTO, APP_LOANS };

// --- STRUKTURY ---
struct Player {
    Vector2 position;
    float baseSpeed;
    float currentSpeed;
    int level;
    Color color;
    
    // Statystyki
    float health;
    float hunger;
    // Energia usunięta
    
    float money; // Może być ujemne
    float debt;
    float hitboxSize; 
    
    // Buffy
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

struct Phone {
    PhoneState state;  
    string correctPin;    
    int selectedIcon;  
    string inputPin; 
    bool showError;
    int currentBet;       
    string gambleInfo;    
    Color infoColor;
    
    // Chwilówki
    int loanOption; 
    string loanInfo;
};

Phone phone = { PHONE_CLOSED, "0000", 0, "", false, 10, "", WHITE, 0, "" };

// --- ZMIENNE GLOBALNE ---
const int screenWidth = 800;
const int screenHeight = 600;

// Gracz startuje z białym kolorem (brak efektów)
Player player = { { 400, 300 }, 200.0f, 200.0f, 1, WHITE, 100.0f, 100.0f, 50.0f, 0.0f, 40.0f, 0.0f, false };

Location currentLocation = POKOJ;
GameScreen currentScreen = GAMEPLAY;
bool isPaused = false;

// Czas
float gameTime = 8.0f; 
int dayCount = 1;

FlankiGame flanki = { AIMING, 0.0f, true, {0,0}, {0,0}, false, 0 };
LevelSystem stats;

// Pomocnicze
int kebabOption = 1;
string notification = ""; 
float notificationTimer = 0.0f;

// --- TEKSTURY ---
Texture2D texturaPiwa;
Texture2D texturaGracza;
Texture2D tloPokoj;
Texture2D tloFlanki;

// --- FUNKCJE POMOCNICZE ---

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

void SetSpawnPoint(int fromSide) {
    if (fromSide == 0) {
        player.position = { (float)screenWidth/2, (float)screenHeight/2 };
    }
    if (fromSide == 1) {
        player.position = { 120, (float)screenHeight/2 }; 
    }
    if (fromSide == 2) {
        player.position = { (float)screenWidth - 120, (float)screenHeight/2 }; 
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
    player.position = { 400, 350 };
    gameTime = 8.0f;
    dayCount++;
    
    if (hospital) {
        player.health = 100.0f;
        player.hunger = 50.0f;
        player.money -= 200.0f; 
        ShowNotification("SZPITAL! Rachunek: 200 zl.");
    } else {
        // Omdlenie z głodu
        player.health = 50.0f;
        player.hunger = 30.0f;
        player.money -= 50.0f; 
        ShowNotification("Zemdlałeś z głodu! -50 zl.");
    }
    // UWAGA: Nie resetujemy długu ani kasy do 0, można mieć debet!
}

// --- UPDATE (LOGIKA) ---
void UpdateGame(float dt) {
    if (IsKeyPressed(KEY_TAB)) isPaused = !isPaused;
    if (isPaused) return;

    if (notificationTimer > 0) notificationTimer -= dt;

    // --- DIALOGI Z NPC ---
    if (currentScreen == DIALOGUE_MODE) {
        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Q)) {
            currentScreen = GAMEPLAY;
            activeNPCIndex = -1;
        }
        return; 
    }

    // --- LOGIKA PRZETRWANIA I CZASU ---
    if (currentScreen == GAMEPLAY) {
        // Czas płynie
        gameTime += dt * 0.5f;
        if (gameTime >= 24.0f) { 
            gameTime = 0.0f; 
            dayCount++; 
        }

        // Głód spada
        player.hunger -= 2.0f * dt;

        // Konsekwencje głodu
        if (player.hunger <= 0.0f) {
            player.hunger = 0.0f;
            player.health -= 5.0f * dt; // Obrażenia
            
            // Miganie na czerwono
            if ((int)(gameTime * 10) % 2 == 0) {
                player.color = RED;
            } else {
                player.color = WHITE;
            }
        } else {
            // Jeśli mamy speed boosta, kolor pomarańczowy, jeśli nie - biały
            if (player.speedBoostTimer <= 0) {
                player.color = WHITE;
            }
        }

        // Śmierć (Szpital)
        if (player.health <= 0.0f) {
            WakeUpInRoom(true); 
        }
    }

    // --- BUFFY ---
    if (player.speedBoostTimer > 0.0f) {
        player.speedBoostTimer -= dt;
        player.currentSpeed = player.baseSpeed * 1.8f; 
        if (player.hunger > 0) player.color = ORANGE; 
    } else {
        player.currentSpeed = player.baseSpeed;
    }
    
    // --- TELEFON ---
    if (currentScreen == GAMEPLAY && IsKeyPressed(KEY_UP)){
        if(phone.state == PHONE_CLOSED) phone.state = MENU1;
    }
    if(IsKeyPressed(KEY_BACKSPACE)){
        if(phone.state != PHONE_CLOSED){
            phone.state = PHONE_CLOSED;
            phone.gambleInfo = ""; 
            phone.loanInfo = "";
        }
    }
    if(phone.state != PHONE_CLOSED && IsKeyPressed(KEY_Q)){
        if(phone.state != MENU1) { 
            phone.state = MENU1; 
            phone.gambleInfo = ""; 
            phone.loanInfo = ""; 
        }
    }

    // --- LOGIKA APLIKACJI W TELEFONIE ---
    if(phone.state == PIN){
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
             if(phone.inputPin == phone.correctPin){
                phone.state = ACCOUNT;
                phone.inputPin = "";
                phone.showError = false;
            } else {
                phone.showError = true;
                phone.inputPin = "";
            }
        }
    }
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
    else if (phone.state == APP_CRYPTO) {
        if (IsKeyPressed(KEY_UP)) phone.currentBet += 10;
        if (IsKeyPressed(KEY_DOWN)) phone.currentBet -= 10;
        
        if (phone.currentBet < 10) phone.currentBet = 10;
        
        // Hazard możliwy tylko z dodatnim saldem (opcjonalnie)
        if (phone.currentBet > player.money && player.money > 0) phone.currentBet = (int)player.money;
        else if (player.money <= 0) phone.currentBet = 0;

        if (IsKeyPressed(KEY_ENTER) && player.money >= phone.currentBet && phone.currentBet > 0) {
            int chance = GetRandomValue(0, 100); 
            if (chance >= 50) { 
                player.money += phone.currentBet;
                phone.gambleInfo = "Wygrana!";
                phone.infoColor = GREEN;
                stats.GainXP(5.0f); 
            } else {
                player.money -= phone.currentBet;
                phone.gambleInfo = "Przegrana...";
                phone.infoColor = RED;
            }
        }
    }
    else if (phone.state == APP_LOANS) {
        if (IsKeyPressed(KEY_DOWN)) phone.loanOption++;
        if (IsKeyPressed(KEY_UP)) phone.loanOption--;
        if (phone.loanOption > 3) phone.loanOption = 0;
        if (phone.loanOption < 0) phone.loanOption = 3;

        if (IsKeyPressed(KEY_ENTER)) {
            if (phone.loanOption == 0) { 
                player.money += 100.0f; player.debt += 120.0f; phone.loanInfo = "Wzieles 100 zl!"; 
            }
            else if (phone.loanOption == 1) { 
                player.money += 500.0f; player.debt += 700.0f; phone.loanInfo = "Wzieles 500 zl!"; 
            }
            else if (phone.loanOption == 2) { 
                player.money += 1000.0f; player.debt += 1500.0f; phone.loanInfo = "Wzieles 1000 zl!"; 
            }
            else if (phone.loanOption == 3) {
                if (player.debt > 0 && player.money > 0) {
                    if (player.money >= player.debt) { 
                        player.money -= player.debt; 
                        player.debt = 0; 
                        phone.loanInfo = "Splacono!"; 
                    } else { 
                        player.debt -= player.money; 
                        player.money = 0; 
                        phone.loanInfo = "Splaciles czesc."; 
                    }
                } else {
                    phone.loanInfo = "Brak kasy/dlugu.";
                }
            }
        }
    }

    // --- MINIGRA FLANKI ---
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
                float throwPowerX = 400.0f + (flanki.power * 200.0f);
                float throwPowerY = -300.0f - (flanki.power * 300.0f);
                flanki.ballVelocity = { throwPowerX, throwPowerY };
            }
        }
        else if (flanki.state == THROWING) {
            flanki.ballPos.x += flanki.ballVelocity.x * dt;
            flanki.ballPos.y += flanki.ballVelocity.y * dt;
            flanki.ballVelocity.y += 980.0f * dt; 

            Rectangle canRect = { 700, 400, 60, 100 }; 
            Vector2 ballCenter = { flanki.ballPos.x + 10, flanki.ballPos.y + 10 };

            if (CheckCollisionCircleRec(ballCenter, 10, canRect)) {
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
        return;
    }
    
    // --- MINIGRA EGZAMIN ---
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
        return; 
    }

    // --- MENU INTERAKCJE (KEBAB, SKLEP, EGZAMIN) ---
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
        return; 
    }

    if (currentScreen == SHOP_MENU) {
        if (IsKeyPressed(KEY_Q)) currentScreen = GAMEPLAY;
        if (IsKeyPressed(KEY_ONE) && player.money >= 20.0f) { 
            player.money -= 20.0f; player.speedBoostTimer = 20.0f; 
        }
        if (IsKeyPressed(KEY_TWO) && player.money >= 100.0f && !player.hasCheatSheet) { 
            player.money -= 100.0f; player.hasCheatSheet = true; 
        }
        return;
    }

    if (currentScreen == EXAM_MENU) {
        if (IsKeyPressed(KEY_Q)) currentScreen = GAMEPLAY;
        if (IsKeyPressed(KEY_ONE)) StartExam(1);
        if (IsKeyPressed(KEY_TWO) && stats.level >= 2) StartExam(2);
        if (IsKeyPressed(KEY_THREE) && stats.level >= 4) StartExam(3);
        return;
    }

    // --- RUCH GRACZA ---
    if (phone.state == PHONE_CLOSED) {
        if (IsKeyDown(KEY_W)){ 
            player.position.y -= player.currentSpeed * dt;
        }
        if (IsKeyDown(KEY_S)){
             player.position.y += player.currentSpeed * dt;
        }
        if (IsKeyDown(KEY_A)){ 
            player.position.x -= player.currentSpeed * dt;
        }
        if (IsKeyDown(KEY_D)) {
            player.position.x += player.currentSpeed * dt;
        }
    }

    // --- GRANICE MAPY ---
    if (currentLocation == POKOJ) {
        if (player.position.x < 50) player.position.x = 50;
        if (player.position.x > 710) player.position.x = 710;
        if (player.position.y < 340) player.position.y = 340;
        if (player.position.y > 560) player.position.y = 560; 
    } 
    else {
        if (player.position.x < 0) player.position.x = 0;
        if (player.position.y < 0) player.position.y = 0;
        if (player.position.x > screenWidth - player.hitboxSize) player.position.x = screenWidth - player.hitboxSize;
        if (player.position.y > screenHeight - player.hitboxSize) player.position.y = screenHeight - player.hitboxSize;
    }

    Rectangle playerRect = { player.position.x, player.position.y, player.hitboxSize, player.hitboxSize };

    // --- ZMIANA LOKACJI I KOLIZJE ---
    if (currentLocation == POKOJ) {
        // Łóżko (sen)
        Rectangle bedRect = { 50, 350, 100, 150 };
        if (CheckCollisionRecs(playerRect, bedRect) && IsKeyPressed(KEY_E)) {
            player.health += 50.0f; if(player.health > 100) player.health = 100;
            player.hunger -= 20.0f; gameTime = 8.0f; dayCount++; 
            ShowNotification("Wyspales sie! Dzien ++");
        }

        Rectangle doorToPark = { (float)screenWidth/2 - 50, (float)screenHeight - 20, 100, 20 };
        if (CheckCollisionRecs(playerRect, doorToPark)) {
            currentLocation = PARK;
            player.position = { (float)screenWidth/2, 50 };
        }
    }
    else if (currentLocation == PARK) {
        Rectangle doorToRoom = { (float)screenWidth/2 - 50, 0, 100, 20 };
        Rectangle doorToCampus = { (float)screenWidth - 20, (float)screenHeight/2 - 50, 20, 100 };
        Rectangle flankiZone = { 200, 400, 40, 40 }; 
        Rectangle kebabRect = { 100, 100, 60, 40 };

        if (CheckCollisionRecs(playerRect, kebabRect)){
            if(IsKeyPressed(KEY_E)){
                currentScreen = KEBAB_MENU;
                kebabOption = 1;
            }
        }

        if (CheckCollisionRecs(playerRect, doorToRoom)) {
            currentLocation = POKOJ;
            player.position = { (float)screenWidth/2, (float)screenHeight - 60 };
        }
        else if (CheckCollisionRecs(playerRect, doorToCampus)) {
            currentLocation = KAMPUS;
            SetSpawnPoint(1);
        }
        
        if (CheckCollisionRecs(playerRect, flankiZone)) {
            if (IsKeyPressed(KEY_E)) {
                currentScreen = MINIGAME_FLANKI;
                ResetFlanki();
            }
        }
    }
    else if (currentLocation == KAMPUS) {
        Rectangle doorToPark = { 0, (float)screenHeight/2 - 50, 20, 100 };
        if (CheckCollisionRecs(playerRect, doorToPark)) {
            currentLocation = PARK;
            SetSpawnPoint(2);
        }
        Rectangle examRect = {350, 300, 100, 60};
        Rectangle shopRect = {600, 100, 60, 80}; 
        
        if (CheckCollisionRecs(playerRect, examRect) && IsKeyPressed(KEY_E)) {
            currentScreen = EXAM_MENU;
        }
        if (CheckCollisionRecs(playerRect, shopRect) && IsKeyPressed(KEY_E)) {
            currentScreen = SHOP_MENU;
        }
    }

    // --- INTERAKCJE Z NPC ---
    for (int i = 0; i < 3; i++) {
        if (npcs[i].mapLocation == currentLocation) {
            Rectangle npcRect = { npcs[i].position.x, npcs[i].position.y, 60, 80 }; 
            if (CheckCollisionRecs(playerRect, npcRect)) {
                if (IsKeyPressed(KEY_E)) {
                    currentScreen = DIALOGUE_MODE;
                    activeNPCIndex = i;
                }
            }
        }
    }
}

// --- DRAW (RYSOWANIE) ---
void DrawGame() {
    BeginDrawing();
    ClearBackground(BLACK);

    // --- MINIGRY ---
    if (currentScreen == MINIGAME_FLANKI) {
        DrawTexturePro(tloFlanki, 
            {0,0,(float)tloFlanki.width, (float)tloFlanki.height}, 
            {0,0,(float)screenWidth, (float)screenHeight}, 
            {0,0}, 0.0f, WHITE);
        
        DrawText("FLANKI", 20, 20, 20, DARKGRAY);
        DrawText(TextFormat("Wynik: %d", flanki.score), 20, 50, 20, BLACK);
        DrawText("[Q] - Powrot", 700, 20, 15, BLACK);

        Rectangle zrodloPiwo = { 0.0f, 0.0f, (float)texturaPiwa.width, (float)texturaPiwa.height };
        Rectangle celPiwo = { 700.0f, 400.0f, 60.0f, 100.0f };
        DrawTexturePro(texturaPiwa, zrodloPiwo, celPiwo, {0,0}, 0.0f, WHITE);

        // GRACZ WE FLANKACH
        Rectangle zrodloGracz = { 0.0f, 0.0f, (float)texturaGracza.width, (float)texturaGracza.height };
        Rectangle celGraczFlanki = { 50.0f, 380.0f, 120.0f, 120.0f }; 
        DrawTexturePro(texturaGracza, zrodloGracz, celGraczFlanki, {0,0}, 0.0f, WHITE);

        DrawCircleV(flanki.ballPos, 10, DARKGRAY);

        if (flanki.state == AIMING) {
            // [POPRAWKA] Tło dla paska
            DrawRectangle(50, 100, 30, 200, LIGHTGRAY);
            DrawRectangle(50, 150, 30, 50, GREEN); 
            int barHeight = (int)(flanki.power * 200);
            DrawRectangle(50, 300 - barHeight, 30, 5, RED);
            DrawText("SPACJA: Rzuc!", 100, 200, 20, BLACK);
        }
        else if (flanki.state == RESULT) {
            if (flanki.playerWon) {
                DrawText("TRAFIONY!", 300, 200, 40, GOLD);
                DrawText("Spacja - rzut", 280, 250, 20, DARKGRAY);
            } else {
                DrawText("PUDLO!", 300, 200, 40, MAROON);
                DrawText("Spacja - rzut", 280, 250, 20, DARKGRAY);
            }
        }
        stats.DrawHUD();

    } else if(currentScreen == MINIGAME_EGZAMIN){
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
    else {
        // --- RYSOWANIE ŚWIATA ---
        if (currentLocation == POKOJ) {
            DrawTexturePro(tloPokoj, 
                {0,0,(float)tloPokoj.width, (float)tloPokoj.height}, 
                {0,0,(float)screenWidth, (float)screenHeight}, 
                {0,0}, 0.0f, WHITE);
            DrawText("POKOJ STUDENTA", 20, 20, 20, DARKGRAY);
            DrawRectangle(50, 350, 100, 150, Fade(BLUE, 0.3f)); 
            DrawText("LOZKO [E]", 60, 400, 10, BLACK);
        }
        else if (currentLocation == PARK) {
            ClearBackground(DARKGREEN); 
            DrawText("PARK", 20, 20, 20, WHITE);
            
            // [POPRAWKA] Ciemność tylko w parku
            if (gameTime >= 20.0f || gameTime < 6.0f) {
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f)); 
            }
            else if (gameTime >= 18.0f) {
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(ORANGE, 0.2f)); 
            }

            DrawRectangle(screenWidth/2 - 50, 0, 100, 20, BROWN); 
            DrawRectangle(screenWidth - 20, screenHeight/2 - 50, 20, 100, GRAY); 
            DrawRectangle(200, 400, 40, 40, GRAY);
            DrawText("Flanki [E]", 195, 380, 10, WHITE);
            DrawRectangle(100, 100, 60, 40, YELLOW);
            DrawText("Kebap", 110, 110, 10, BLACK);
        }
        else if (currentLocation == KAMPUS) {
            ClearBackground(LIGHTGRAY);
            DrawText("KAMPUS", 20, 20, 20, DARKBLUE);
            DrawRectangle(0, screenHeight/2 - 50, 20, 100, DARKGREEN); 
            DrawRectangle(350, 300, 100, 60, PURPLE);
            DrawText("EGZAMIN [E]", 365, 320, 10, WHITE);
            DrawRectangle(600, 100, 60, 80, RED); 
            DrawText("SKLEP [E]", 595, 80, 10, WHITE);
        }

        // --- RYSOWANIE GRACZA ---
        
        float drawSize = 120.0f; // Domyślny rozmiar (Park)
        
        if (currentLocation == POKOJ || currentLocation == KAMPUS) {
            drawSize = 300.0f; 
        }

        // Centrowanie rysunku względem stóp (hitboxa)
        float offsetXY = (drawSize - player.hitboxSize) / 2.0f;

        Rectangle zrodloStudent = { 0.0f, 0.0f, (float)texturaGracza.width, (float)texturaGracza.height };
        
        Rectangle celStudent = { 
            player.position.x - offsetXY, 
            player.position.y - offsetXY, 
            drawSize, 
            drawSize 
        };
        
        DrawTexturePro(texturaGracza, zrodloStudent, celStudent, {0,0}, 0.0f, player.color);
        
        // --- NPC ---
        for (int i = 0; i < 3; i++) {
            if (npcs[i].mapLocation == currentLocation) {
                // Rysujemy NPC jako prostokąt z kolorem (można podmienić na teksturę)
                DrawTexturePro(texturaGracza, 
                    {0,0,(float)texturaGracza.width,(float)texturaGracza.height}, 
                    {npcs[i].position.x, npcs[i].position.y, 100, 100}, 
                    {0,0}, 0.0f, npcs[i].color);
                DrawText(npcs[i].name.c_str(), npcs[i].position.x, npcs[i].position.y - 20, 10, WHITE);
            }
        }

        // --- HUD ---
        stats.DrawHUD();
        DrawText(TextFormat("DZIEN %d | %02d:00", dayCount, (int)gameTime), screenWidth - 180, 20, 20, WHITE);
        
        float barWidth = 220.0f;
        float barHeight = 20.0f;
        float x = GetScreenWidth() - barWidth - 20; 
        float y = 50.0f; 

        // Pasek Życia
        DrawRectangle(x, y, barWidth, barHeight, Fade(BLACK, 0.6f));
        DrawRectangle(x, y, barWidth * (player.health / 100.0f), barHeight, RED);
        DrawRectangleLines(x, y, barWidth, barHeight, WHITE);
        DrawText("ZYCIE", x + 5, y + 2, 10, WHITE);

        // Pasek Głodu
        y += 25;
        DrawRectangle(x, y, barWidth, barHeight, Fade(BLACK, 0.6f));
        float hungerPercent = player.hunger / 100.0f; 
        DrawRectangle(x, y, barWidth * hungerPercent, barHeight, (player.hunger < 20.0f) ? ORANGE : GREEN);
        DrawRectangleLines(x, y, barWidth, barHeight, WHITE);
        DrawText("GLOD", x + 5, y + 2, 10, WHITE);

        if (notificationTimer > 0) {
            DrawText(notification.c_str(), screenWidth/2 - MeasureText(notification.c_str(), 30)/2, 100, 30, RED);
        }

        // --- OKNA DIALOGOWE I MENU ---
        if (currentScreen == DIALOGUE_MODE && activeNPCIndex != -1) {
            DrawRectangle(0, 400, screenWidth, 200, Fade(BLACK, 0.8f));
            DrawRectangleLines(0, 400, screenWidth, 200, WHITE);
            DrawText(npcs[activeNPCIndex].name.c_str(), 50, 420, 30, npcs[activeNPCIndex].color);
            DrawText(npcs[activeNPCIndex].dialogue.c_str(), 50, 470, 20, WHITE);
            DrawText("[E] Zakoncz", 600, 560, 15, GRAY);
        }

        if (currentScreen == KEBAB_MENU) {
            DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK,0.7f));
            DrawRectangle(200,150,400,300,RAYWHITE); 
            DrawRectangleLines(200,150,400,300,BLACK);
            DrawText("U SZWAGRA",330,165,20,BLACK);
            Color c1=(kebabOption==0)?RED:BLACK; 
            Color c2=(kebabOption==1)?RED:BLACK; 
            Color c3=(kebabOption==2)?RED:BLACK;
            DrawText("1. Frytki (8zl, Glod+)",240,260,20,c1); 
            DrawText("2. Rollo (18zl, Glod++)",240,300,20,c2); 
            DrawText("3. Kubel XXL (30zl, Glod MAX, HP+)",240,340,20,c3);
            DrawText(TextFormat("Kasa: %.2f",player.money),240,400,20,DARKGREEN);
        }
        if (currentScreen == SHOP_MENU) {
            DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK,0.7f)); 
            DrawRectangle(100,100,600,400,DARKGRAY); 
            DrawRectangleLines(100,100,600,400,WHITE);
            DrawText("SKLEP",250,120,30,WHITE); 
            DrawText("1. Energetyk (20zl)",150,200,20,GREEN); 
            DrawText("2. Sciaga (100zl)",150,250,20,YELLOW);
            DrawText(TextFormat("Kasa: %.2f",player.money),150,350,20,WHITE);
        }
        if (currentScreen == EXAM_MENU) {
            DrawRectangle(0,0,screenWidth,screenHeight,Fade(BLACK,0.7f)); 
            DrawRectangle(100,100,600,400,RAYWHITE); 
            DrawRectangleLines(100,100,600,400,BLACK);
            DrawText("WYBIERZ EGZAMIN [1-3]",200,120,30,DARKBLUE);
            DrawText("1. Latwy (50zl)",150,200,20,DARKGREEN);
            DrawText(stats.level>=2?"2. Sredni (150zl)":"2. Sredni (Wymaga LVL 2)",150,250,20,stats.level>=2?DARKGREEN:GRAY);
            DrawText(stats.level>=4?"3. Trudny (500zl)":"3. Trudny (Wymaga LVL 4)",150,300,20,stats.level>=4?DARKGREEN:GRAY);
        }

        // --- TELEFON ---
        if (phone.state != PHONE_CLOSED) {
            int px = 560, py = 180;
            if (phone.state == MENU1) {
                DrawRectangle(px,py,220,400,DARKGRAY); 
                DrawRectangleLines(px,py,220,400,BLACK);
                DrawRectangle(575,210,45,45,RAYWHITE); 
                DrawText("BANK",580,225,10,BLACK); 
                if(phone.selectedIcon==0) DrawRectangleLines(575,210,45,45,RED);
                
                DrawRectangle(630,210,45,45,BLACK); 
                DrawText("KRYPTO",632,225,10,GREEN); 
                if(phone.selectedIcon==1) DrawRectangleLines(630,210,45,45,RED);
                
                DrawRectangle(685,210,45,45,LIGHTGRAY); 
                DrawText("WONGA",690,225,10,BLACK); 
                if(phone.selectedIcon==2) DrawRectangleLines(685,210,45,45,RED);
            } 
            else if (phone.state == PIN){
                DrawRectangle(560, 180, 220, 400, LIGHTGRAY);
                DrawRectangleLines(560, 180, 220, 400, BLACK);
                DrawText("WPROWADZ PIN:", 580, 200, 15, BLACK);
                DrawText(phone.inputPin.c_str(), 600, 250, 20, BLACK);
                if(phone.showError) DrawText("BLEDNY PIN!", 580, 300, 15, RED);
            } 
            else if (phone.state == ACCOUNT){
                DrawRectangle(560, 180, 220, 400, RAYWHITE);
                DrawRectangleLines(560, 180, 220, 400, BLACK);
                DrawText("SALDO KONTA:", 580, 220, 10, DARKGRAY);
                DrawText(TextFormat("%.2f zl", player.money), 590, 240, 20, GREEN);
                DrawText(TextFormat("DLUG: %.2f", player.debt), 590, 280, 20, RED);
                DrawText("[Q] Wroc", 630, 550, 10, LIGHTGRAY);
            } 
            else if (phone.state == APP_CRYPTO){
                DrawRectangle(560, 180, 220, 400, BLACK);
                DrawRectangleLines(560, 180, 220, 400, WHITE);
                DrawText("SZYBKIE KRYPTO", 590, 200, 20, PURPLE);
                DrawText("Twoje konto:", 580, 240, 15, WHITE);
                DrawText(TextFormat("%.2f zl", player.money), 580, 260, 20, GREEN);
                DrawText("STAWKA (Gora/Dol):", 580, 300, 10, LIGHTGRAY);
                DrawRectangle(580, 315, 180, 40, DARKGRAY);
                DrawText(TextFormat("%d zl", phone.currentBet), 630, 325, 20, WHITE);
                DrawRectangle(580, 370, 180, 40, GOLD);
                DrawText("INWESTUJ [ENTER]", 595, 382, 15, BLACK);
                DrawText(phone.gambleInfo.c_str(), 580, 430, 15, phone.infoColor);
                DrawText("[Q] Wroc", 640, 550, 10, GRAY);
            }
            else if (phone.state == APP_LOANS) {
                 DrawRectangle(px,py,220,400,LIGHTGRAY); 
                 DrawRectangleLines(px,py,220,400,BLACK);
                 DrawText("CHWILOWKI",580,200,20,BLACK);
                 DrawText(phone.loanInfo.c_str(),580,240,10,BLUE);
                 
                 float prowizja = 0;
                 if(phone.loanOption==0) prowizja = 20;
                 if(phone.loanOption==1) prowizja = 200;
                 if(phone.loanOption==2) prowizja = 500;
                 
                 DrawText(TextFormat("Stan konta: %.2f", player.money), 580, 410, 10, DARKGREEN);
                 
                 Color c0=BLACK, c1=BLACK, c2=BLACK, c3=BLACK;
                 if(phone.loanOption==0) c0=RED; if(phone.loanOption==1) c1=RED; if(phone.loanOption==2) c2=RED; if(phone.loanOption==3) c3=RED;
                 
                 DrawText("1. Mala (100)",570,270,10,c0);
                 DrawText("2. Srednia (500)",570,290,10,c1);
                 DrawText("3. Duza (1000)",570,310,10,c2);
                 DrawText("4. SPLAC DLUG",570,350,10,c3);
                 
                 if (phone.loanOption >= 0 && phone.loanOption <= 2) {
                     DrawText(TextFormat("Prowizja: %.0f zl", prowizja), 570, 380, 10, RED);
                 }
            }
        }

        if (isPaused) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
            DrawText("PAUZA", 350, 280, 40, WHITE);
        }
    }

    EndDrawing();
}

int main() {
    InitWindow(screenWidth, screenHeight, "Student RPG & Flanki");
    SetTargetFPS(60);

    texturaPiwa = LoadTexture("assets/piwo.png");
    texturaGracza = LoadTexture("assets/postac.png");
    tloPokoj = LoadTexture("assets/akademik.png");
    tloFlanki = LoadTexture("assets/tloflanki.png");

    while (!WindowShouldClose()) {
        UpdateGame(GetFrameTime());
        DrawGame();
    }

    UnloadTexture(texturaPiwa);
    UnloadTexture(texturaGracza);
    UnloadTexture(tloPokoj);
    UnloadTexture(tloFlanki);

    CloseWindow();
    return 0;
}