#include "raylib.h"
#include <string>
#include <vector>
#include <cmath> 
#include <cstdio> 

using namespace std;

// --- STRUKTURA PYTAŃ DO EGZAMINU ---
struct Question {
    string text;       
    string answerA;    
    string answerB;    
    int correctKey;    
};

Question examQuestions[] = {
    {"Ile to jest 2 + 2?", "3", "4", KEY_TWO},
    {"Stolica Polski to?", "Warszawa", "Krakow", KEY_ONE},
    {"Rok bitwy pod Grunwaldem?", "1410", "1920", KEY_ONE},
    {"Najlepszy przyjaciel studenta?", "Sesja", "Piwo", KEY_TWO},
    {"Wzor na pole kwadratu?", "a * a", "2 * a", KEY_ONE}
};
int currentQuestionIndex = 0; 

// --- SYSTEM LEVELOWANIA ---
struct LevelSystem {
    int level = 8;              
    float currentXP = 0.0f;     
    float requiredXP = 20.0f;   

    void GainXP(float amount) {
        currentXP += amount;
        while (currentXP >= requiredXP) {
            currentXP -= requiredXP;
            level++;
            if(level<=10) requiredXP *= 1.5; 
            else requiredXP *= 2.5; 
        }
    }

    void DrawHUD() {
        int screenW = GetScreenWidth();
        float barW = 220.0f;
        float barH = 30.0f;
        float margin = 20.0f;
        float x = screenW - barW - margin;
        float y = margin;

        DrawRectangle(x, y, barW, barH, Fade(BLACK, 0.6f));
        float progress = currentXP / requiredXP;
        if (progress > 1.0f) progress = 1.0f; 
        
        DrawRectangle(x, y, barW * progress, barH, ORANGE);
        DrawRectangleLines(x, y, barW, barH, WHITE);
        DrawText(TextFormat("POZIOM %d", level), x, y - 20, 20, BLACK);
        
        const char* xpText = TextFormat("%d / %d XP", (int)currentXP, (int)requiredXP);
        int textWidth = MeasureText(xpText, 10);
        DrawText(xpText, x + (barW/2) - (textWidth/2), y + 10, 10, WHITE);
    }
};

// --- ENUMY ---
enum GameScreen { MENU, GAMEPLAY, MINIGAME_FLANKI, MINIGAME_EGZAMIN };
enum Location { POKOJ, PARK, KAMPUS };
enum FlankiState { AIMING, THROWING, RESULT };
enum PhoneState { PHONE_CLOSED, MENU1, PIN, ACCOUNT };

// --- STRUKTURY ---
struct Player {
    Vector2 position;
    float speed;
    int level;
    Color color;
    float hunger;
    float money;
    float hitboxSize; // Rozmiar fizyczny (niewidzialny kwadrat kolizji)
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
};

Phone phone = { PHONE_CLOSED, "0000", 0, "", false };

// --- ZMIENNE GLOBALNE ---
const int screenWidth = 800;
const int screenHeight = 600;

// Hitbox zostawiamy mały (40px), żeby nie blokował się w drzwiach
Player player = { { 400, 300 }, 200.0f, 1, BLUE, 100.0f, 20.0f, 40.0f };

Location currentLocation = POKOJ;
GameScreen currentScreen = GAMEPLAY;
bool isPaused = false;

FlankiGame flanki = { AIMING, 0.0f, true, {0,0}, {0,0}, false, 0 };
LevelSystem stats;

// --- TEKSTURY ---
Texture2D texturaPiwa;
Texture2D texturaGracza;
Texture2D tloPokoj;
Texture2D tloFlanki;

// --- FUNKCJE POMOCNICZE ---

void ResetFlanki() {
    flanki.state = AIMING;
    flanki.power = 0.0f;
    flanki.powerGoingUp = true;
    flanki.ballPos = { 100, 450 }; 
    flanki.ballVelocity = { 0, 0 };
}

void SetSpawnPoint(int fromSide) {
    if (fromSide == 0) player.position = { (float)screenWidth/2, (float)screenHeight/2 };
    
    // Spawn pointy odsunięte od drzwi (120px), żeby nie wchodzić w pętlę
    if (fromSide == 1) player.position = { 120, (float)screenHeight/2 }; 
    if (fromSide == 2) player.position = { (float)screenWidth - 120, (float)screenHeight/2 }; 
}

// --- UPDATE (LOGIKA) ---
void UpdateGame(float dt) {
    if (IsKeyPressed(KEY_TAB)) isPaused = !isPaused;
    if (isPaused) return;
    
    // Telefon
    if(IsKeyPressed(KEY_UP)){
        if(phone.state == PHONE_CLOSED) phone.state = MENU1;
    }
    if(IsKeyPressed(KEY_BACKSPACE)){
        if(phone.state == MENU1 || phone.state == PIN || phone.state == ACCOUNT) phone.state = PHONE_CLOSED;
    }
    if(phone.state == MENU1 || phone.state == PIN || phone.state == ACCOUNT){
        if(IsKeyPressed(KEY_Q)){
            if(phone.state == ACCOUNT) phone.state = MENU1; 
        }
    }
    if(phone.state == MENU1 && IsKeyPressed(KEY_ENTER) && phone.selectedIcon == 0){
        phone.state = PIN;
    }
    else if(phone.state == PIN){
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
        if (IsKeyPressed(examQuestions[currentQuestionIndex].correctKey)) {
            player.money += 50;        
            currentScreen = GAMEPLAY;  
        }
        return; 
    }

    // --- RUCH GRACZA ---
    if (IsKeyDown(KEY_W)){ 
        player.position.y -= player.speed * dt;
        player.hunger -= 0.5f * dt;
    }
    if (IsKeyDown(KEY_S)){
         player.position.y += player.speed * dt;
         player.hunger -= 0.5f * dt;
    }
    if (IsKeyDown(KEY_A)){ 
        player.position.x -= player.speed * dt;
        player.hunger -= 0.5f * dt;
    }
    if (IsKeyDown(KEY_D)) {
        player.position.x += player.speed * dt;
        player.hunger -= 0.5f * dt;
    }

    // --- GRANICE MAPY (POPRAWIONE) ---
    
    if (currentLocation == POKOJ) {
        // POSZERZONE GRANICE - TERAZ MOŻNA DOJŚĆ DO DRZWI
        if (player.position.x < 50) player.position.x = 50;
        if (player.position.x > 710) player.position.x = 710;
        if (player.position.y < 340) player.position.y = 340;
        
        // Zmieniono z 530 na 560, żeby stopa gracza dotknęła drzwi (Y=580)
        if (player.position.y > 560) player.position.y = 560; 
    } 
    else {
        // Granice dla Parku i Kampusu
        if (player.position.x < 0) player.position.x = 0;
        if (player.position.y < 0) player.position.y = 0;
        if (player.position.x > screenWidth - player.hitboxSize) player.position.x = screenWidth - player.hitboxSize;
        if (player.position.y > screenHeight - player.hitboxSize) player.position.y = screenHeight - player.hitboxSize;
    }

    Rectangle playerRect = { player.position.x, player.position.y, player.hitboxSize, player.hitboxSize };

    // --- ZMIANA LOKACJI ---
    if (currentLocation == POKOJ) {
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
                if(player.money >= 15.0f){
                    player.money -= 15.0f;
                    player.hunger += 30.0f;
                    if (player.hunger > 100.0f) player.hunger = 100.0f;
                }
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
        
        if (CheckCollisionRecs(playerRect, examRect) && IsKeyPressed(KEY_E)) {
            currentScreen = MINIGAME_EGZAMIN;
            currentQuestionIndex = GetRandomValue(0, 4);
        }
    }
}

// --- DRAW (RYSOWANIE) ---
void DrawGame() {
    BeginDrawing();
    ClearBackground(BLACK);

    if (phone.state == MENU1) {
        DrawRectangle(560, 180, 220, 400, DARKGRAY);
        DrawRectangleLines(560, 180, 220, 400, BLACK);
        DrawRectangle(590, 210, 50, 50, BLUE);
        DrawText("BANK", 595, 225, 10, WHITE);
        if (phone.selectedIcon == 0) DrawRectangleLines(590, 210, 50, 50, RED);
    } else if(phone.state == PIN){
        DrawRectangle(560, 180, 220, 400, LIGHTGRAY);
        DrawRectangleLines(560, 180, 220, 400, BLACK);
        DrawText("WPROWADZ PIN:", 580, 200, 15, BLACK);
        DrawText(phone.inputPin.c_str(), 600, 250, 20, BLACK);
        if(phone.showError) DrawText("BLEDNY PIN!", 580, 300, 15, RED);
    } else if(phone.state == ACCOUNT){
        DrawRectangle(560, 180, 220, 400, RAYWHITE);
        DrawRectangleLines(560, 180, 220, 400, BLACK);
        DrawText("SALDO KONTA:", 580, 220, 10, DARKGRAY);
        DrawText(TextFormat("%.2f zl", player.money), 590, 240, 20, GREEN);
        DrawText("[Q] Wroc", 630, 550, 10, LIGHTGRAY);
    }

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
    }
    else {
        // --- RYSOWANIE ŚWIATA ---
        if (currentLocation == POKOJ) {
            DrawTexturePro(tloPokoj, 
                {0,0,(float)tloPokoj.width, (float)tloPokoj.height}, 
                {0,0,(float)screenWidth, (float)screenHeight}, 
                {0,0}, 0.0f, WHITE);
            DrawText("POKOJ STUDENTA", 20, 20, 20, DARKGRAY);
        }
        else if (currentLocation == PARK) {
            ClearBackground(DARKGREEN); 
            DrawText("PARK", 20, 20, 20, WHITE);
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
        }

        // --- RYSOWANIE GRACZA (ZMIENNY ROZMIAR) ---
        
        float drawSize = 120.0f; // Domyślny rozmiar (Park)
        
        // W Pokoju i na Kampusie postać jest 2.5x większa
        if (currentLocation == POKOJ || currentLocation == KAMPUS) {
            drawSize = 300.0f; // 120 * 2.5 = 300
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
        
        DrawTexturePro(texturaGracza, zrodloStudent, celStudent, {0,0}, 0.0f, WHITE);
        
        stats.DrawHUD();
        
        Color hungerColor = GREEN;
        if (player.hunger < 50.0f) hungerColor = ORANGE;
        if (player.hunger < 20.0f) hungerColor = RED;

        float barWidth = 220.0f;
        float barHeight = 30.0f;
        float x = GetScreenWidth() - barWidth - 20; 
        float y = 60.0f; 

        DrawRectangle(x, y, barWidth, barHeight, Fade(BLACK, 0.6f));
        float hungerPercent = player.hunger / 100.0f; 
        DrawRectangle(x, y, barWidth * hungerPercent, barHeight, hungerColor);
        DrawRectangleLines(x, y, barWidth, barHeight, WHITE);
        DrawText("GLOD", x + 10, y + 8, 10, WHITE);
        DrawText(TextFormat("%d%%", (int)player.hunger), x + barWidth - 40, y + 8, 10, WHITE);

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