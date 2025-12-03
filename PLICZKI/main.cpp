#include "raylib.h"
#include <string>
#include <cmath> // Do fizyki rzutu
#include <cstdio> // Do TextFormat/sprintf

// --- NOWOŚĆ: SYSTEM LEVELOWANIA ---
struct LevelSystem {
    int level = 8;              // Startowy poziom
    float currentXP = 0.0f;     // Obecne XP
    float requiredXP = 20.0f;   // XP potrzebne na pierwszy awans
    const float multiplier = 1.5f; // Mnożnik (1.5x trudniej co poziom)

    // Funkcja dodająca XP (wywołaj ją, gdy trafisz)
    void GainXP(float amount) {
        currentXP += amount;
        
        // Sprawdzamy czy awans (pętla while na wypadek dużej ilości XP na raz)
        while (currentXP >= requiredXP) {
            currentXP -= requiredXP;      // Odejmij zużyte punkty
            level++;
            //nie uzywanie multiplier juz ale algorytm do poziomow zeby było trudniej na wyszych poziomach 
            if(level<=10){
                requiredXP = requiredXP * 1.5; 
            }
            else{
                requiredXP = requiredXP * 2.5; // Poziom 11+ wymaga 2.5 XP
            }                    
                 
        }
    }

    // Funkcja rysująca interfejs (UI)
    void DrawHUD() {
        int screenW = GetScreenWidth();
        
        // Wymiary paska
        float barW = 220.0f;
        float barH = 30.0f;
        float margin = 20.0f;

        // Pozycja: Prawy Górny Róg
        float x = screenW - barW - margin;
        float y = margin;

        // 1. Tło paska (półprzezroczyste czarne)
        DrawRectangle(x, y, barW, barH, Fade(BLACK, 0.6f));

        // 2. Wypełnienie (zależy od postępu)
        float progress = currentXP / requiredXP;
        // Zabezpieczenie, żeby pasek nie wyszedł poza ramkę przy animacji
        if (progress > 1.0f) progress = 1.0f; 
        
        DrawRectangle(x, y, barW * progress, barH, ORANGE);

        // 3. Ramka
        DrawRectangleLines(x, y, barW, barH, WHITE);

        // 4. Teksty
        DrawText(TextFormat("POZIOM %d", level), x, y - 20, 20, BLACK); // Nad paskiem
        
        // Tekst na środku paska (np. "10 / 20 XP")
        const char* xpText = TextFormat("%d / %d XP", (int)currentXP, (int)requiredXP);
        int textWidth = MeasureText(xpText, 10);
        DrawText(xpText, x + (barW/2) - (textWidth/2), y + 10, 10, WHITE);
    }
};

// --- ENUMY ---
enum GameScreen {
    MENU,
    GAMEPLAY,
    MINIGAME_FLANKI
};

enum Location {
    POKOJ,
    PARK,
    KAMPUS
};

enum FlankiState {
    AIMING, // Celowanie paskiem
    THROWING, // Lot kamienia
    RESULT // Wynik (Trafiles/Pudlo)
};

// --- STRUKTURY ---
struct Player {
    Vector2 position;
    float speed;
    int level;
    Color color;
};

struct FlankiGame {
    FlankiState state;
    float power;         // Moc rzutu (0.0 - 1.0)
    bool powerGoingUp;   // Czy pasek rośnie?
    Vector2 ballPos;     // Pozycja kamienia
    Vector2 ballVelocity;// Prędkość kamienia
    bool playerWon;      // Czy trafiliśmy
    int score;
};

// --- ZMIENNE GLOBALNE ---
const int screenWidth = 800;
const int screenHeight = 600;

Player player = { { 400, 300 }, 200.0f, 1, BLUE };
Location currentLocation = POKOJ;
GameScreen currentScreen = GAMEPLAY;
bool isPaused = false;

// Inicjalizacja minigry
FlankiGame flanki = { AIMING, 0.0f, true, {0,0}, {0,0}, false, 0 };

// NOWOŚĆ: Inicjalizacja systemu levelowania
LevelSystem stats;

// --- FUNKCJE POMOCNICZE ---

void ResetFlanki() {
    flanki.state = AIMING;
    flanki.power = 0.0f;
    flanki.powerGoingUp = true;
    flanki.ballPos = { 100, 450 }; // Pozycja startowa (ręka gracza)
    flanki.ballVelocity = { 0, 0 };
}

// Funkcja ustawiająca gracza w bezpiecznej odległości od drzwi
void SetSpawnPoint(int fromSide) {
    // 0=środek, 1=z lewej (np. wejście na kampus), 2=z prawej (wejście do parku)
    if (fromSide == 0) player.position = { (float)screenWidth/2, (float)screenHeight/2 };
    
    // NAPRAWA BŁĘDU: Odsunęliśmy spawna na X=60, bo drzwi są od 0 do 30.
    // Wcześniej spawn nakładał się na drzwi.
    if (fromSide == 1) player.position = { 60, (float)screenHeight/2 }; 
    
    if (fromSide == 2) player.position = { (float)screenWidth - 60, (float)screenHeight/2 }; 
}

// --- UPDATE (LOGIKA) ---
void UpdateGame(float dt) {
    // PAUZA
    if (IsKeyPressed(KEY_TAB)) isPaused = !isPaused;
    if (isPaused) return;

    // --- LOGIKA MINIGRY FLANKI ---
    if (currentScreen == MINIGAME_FLANKI) {
        if (IsKeyPressed(KEY_Q)) {
            currentScreen = GAMEPLAY; // Wyjście z minigry
        }

        if (flanki.state == AIMING) {
            // Pasek mocy lata góra-dół
            float speed = 1.5f * dt;
            if (flanki.powerGoingUp) flanki.power += speed;
            else flanki.power -= speed;

            if (flanki.power >= 1.0f) flanki.powerGoingUp = false;
            if (flanki.power <= 0.0f) flanki.powerGoingUp = true;

            // Rzut spacją
            if (IsKeyPressed(KEY_SPACE)) {
                flanki.state = THROWING;
                // Obliczamy wektor rzutu bazując na mocy
                // X leci zawsze do przodu, Y zależy od mocy (im mocniej, tym wyżej/dalej)
                float throwPowerX = 400.0f + (flanki.power * 200.0f); // Siła pozioma
                float throwPowerY = -300.0f - (flanki.power * 300.0f); // Siła w górę (minus bo Y=0 jest u góry)
                
                flanki.ballVelocity = { throwPowerX, throwPowerY };
            }
        }
        else if (flanki.state == THROWING) {
            // Fizyka lotu
            flanki.ballPos.x += flanki.ballVelocity.x * dt;
            flanki.ballPos.y += flanki.ballVelocity.y * dt;
            
            flanki.ballVelocity.y += 980.0f * dt; // Grawitacja

            // Puszka (Cel) stoi na X=700, Y=450 (rozmiar 30x50)
            Rectangle canRect = { 700, 450, 30, 50 };
            Vector2 ballCenter = { flanki.ballPos.x + 10, flanki.ballPos.y + 10 };

            // Sprawdzenie trafienia
            if (CheckCollisionCircleRec(ballCenter, 10, canRect)) {
                flanki.playerWon = true;
                flanki.score++;
                
                //  DODAWANIE EXP ---
                // Dodajemy XP tylko jeśli trafiliśmy
                // tu jest ustawione na 10.0f, ale możesz zmienić według uznania(Np w innych minigrach jakby co)
                stats.GainXP(10.0f); 
                // -----------------------------

                flanki.state = RESULT;
            }

            // Jeśli spadnie na ziemię (podłoga na Y=500)
            if (flanki.ballPos.y > 500) {
                flanki.playerWon = false;
                flanki.state = RESULT;
            }
        }
        else if (flanki.state == RESULT) {
            if (IsKeyPressed(KEY_SPACE)) {
                ResetFlanki(); // Restart rzutu
            }
        }
        return; // Jeśli jesteśmy w minigrze, nie robimy ruchu postacią poniżej
    }

    // --- LOGIKA NORMALNA (CHODZENIE) ---
    
    // Ruch
    if (IsKeyDown(KEY_W)) player.position.y -= player.speed * dt;
    if (IsKeyDown(KEY_S)) player.position.y += player.speed * dt;
    if (IsKeyDown(KEY_A)) player.position.x -= player.speed * dt;
    if (IsKeyDown(KEY_D)) player.position.x += player.speed * dt;

    // Granice ekranu
    if (player.position.x < 0) player.position.x = 0;
    if (player.position.y < 0) player.position.y = 0;
    if (player.position.x > screenWidth - 40) player.position.x = screenWidth - 40;
    if (player.position.y > screenHeight - 40) player.position.y = screenHeight - 40;

    Rectangle playerRect = { player.position.x, player.position.y, 40, 40 };

    // --- OBSŁUGA LOKACJI ---
    if (currentLocation == POKOJ) {
        Rectangle doorToPark = { (float)screenWidth/2 - 50, (float)screenHeight - 20, 100, 20 };
        if (CheckCollisionRecs(playerRect, doorToPark)) {
            currentLocation = PARK;
            player.position = { (float)screenWidth/2, 50 };
        }
    }
    else if (currentLocation == PARK) {
        Rectangle doorToRoom = { (float)screenWidth/2 - 50, 0, 100, 20 };
        // Drzwi na kampus (prawa krawędź)
        Rectangle doorToCampus = { (float)screenWidth - 20, (float)screenHeight/2 - 50, 20, 100 };
        
        // Strefa Flanek (Kamień)
        Rectangle flankiZone = { 200, 400, 40, 40 }; // Kamień w parku

        if (CheckCollisionRecs(playerRect, doorToRoom)) {
            currentLocation = POKOJ;
            player.position = { (float)screenWidth/2, (float)screenHeight - 60 };
        }
        else if (CheckCollisionRecs(playerRect, doorToCampus)) {
            currentLocation = KAMPUS;
            SetSpawnPoint(1); // Wchodzimy od lewej strony kampusu
        }
        
        // Wejście do minigry
        if (CheckCollisionRecs(playerRect, flankiZone)) {
            DrawText("Wcisnij [E] aby grac we Flanki", 150, 380, 20, BLACK); // Rysujemy tekst tutaj "na szybko"
            if (IsKeyPressed(KEY_E)) {
                currentScreen = MINIGAME_FLANKI;
                ResetFlanki();
            }
        }
    }
    else if (currentLocation == KAMPUS) {
        // Drzwi powrotne do parku (lewa krawędź)
        // NAPRAWA: Sprawdzamy kolizję tylko na pierwszych 20 pikselach
        Rectangle doorToPark = { 0, (float)screenHeight/2 - 50, 20, 100 };
        
        if (CheckCollisionRecs(playerRect, doorToPark)) {
            currentLocation = PARK;
            SetSpawnPoint(2); // Wchodzimy od prawej strony parku
        }
    }
}

// --- DRAW (RYSOWANIE) ---
void DrawGame() {
    BeginDrawing();
    ClearBackground(BLACK);

    if (currentScreen == MINIGAME_FLANKI) {
        ClearBackground(SKYBLUE);
        DrawRectangle(0, 500, screenWidth, 100, DARKGREEN); // Trawa
        
        DrawText("FLANKI - STUDENCKA GRA", 20, 20, 20, DARKGRAY);
        DrawText(TextFormat("Wynik: %d", flanki.score), 20, 50, 20, BLACK);
        DrawText("[Q] - Powrot", 700, 20, 15, BLACK);

        // Rysujemy Puszke
        DrawRectangle(700, 450, 30, 50, RED); 
        DrawText("PIWO", 702, 460, 10, WHITE);

        // Rysujemy Gracza (rzucającego)
        DrawRectangle(50, 420, 40, 80, BLUE);

        // Rysujemy Kamień/Piłkę
        DrawCircleV(flanki.ballPos, 10, DARKGRAY);

        if (flanki.state == AIMING) {
            // Pasek mocy
            DrawRectangle(50, 100, 30, 200, LIGHTGRAY); // Tło paska
            
            // Strefa trafienia (zielona) - tam trzeba celować
            DrawRectangle(50, 150, 30, 50, GREEN); 

            // Wskaźnik mocy (czerwony)
            // Im większa moc, tym wyżej wskaźnik na pasku
            int barHeight = (int)(flanki.power * 200);
            DrawRectangle(50, 300 - barHeight, 30, 5, RED);

            DrawText("SPACJA: Rzuc!", 100, 200, 20, BLACK);
        }
        else if (flanki.state == RESULT) {
            if (flanki.playerWon) {
                DrawText("TRAFIONY! JEST!", 300, 200, 40, GOLD);
                DrawText("Spacja aby rzucic ponownie", 280, 250, 20, DARKGRAY);
            } else {
                DrawText("PUDLO! KARNY!", 300, 200, 40, MAROON);
                DrawText("Spacja aby rzucic ponownie", 280, 250, 20, DARKGRAY);
            }
        }
        
        // --- NOWOŚĆ: RYSOWANIE PASKA LEVELOWANIA ---
        stats.DrawHUD();
        // -------------------------------------------

    } 
    else {
        // --- RYSOWANIE ŚWIATA ---
        if (currentLocation == POKOJ) {
            ClearBackground(BEIGE);
            DrawText("POKOJ STUDENTA", 20, 20, 20, DARKGRAY);
            DrawRectangle(screenWidth/2 - 50, screenHeight - 20, 100, 20, BROWN);
        }
        else if (currentLocation == PARK) {
            ClearBackground(DARKGREEN);
            DrawText("PARK", 20, 20, 20, WHITE);
            
            DrawRectangle(screenWidth/2 - 50, 0, 100, 20, BROWN); // Do pokoju
            DrawRectangle(screenWidth - 20, screenHeight/2 - 50, 20, 100, GRAY); // Do kampusu
            
            // Obiekt Flanki (Kamień)
            DrawRectangle(200, 400, 40, 40, GRAY);
            DrawText("Flanki", 195, 380, 10, WHITE);
        }
        else if (currentLocation == KAMPUS) {
            ClearBackground(LIGHTGRAY);
            DrawText("KAMPUS", 20, 20, 20, DARKBLUE);
            DrawRectangle(0, screenHeight/2 - 50, 20, 100, DARKGREEN); // Do parku
        }

        // Rysuj gracza
        DrawRectangle((int)player.position.x, (int)player.position.y, 40, 40, player.color);
        
        // --- NOWOŚĆ: RYSOWANIE PASKA LEVELOWANIA RÓWNIEŻ W ŚWIECIE GRY ---
        // (Jeśli chcesz, żeby pasek było widać też jak chodzisz, zostaw to. 
        //  Jeśli ma być tylko we flankach, usuń poniższą linijkę)
        stats.DrawHUD();
        
        // Pauza
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

    while (!WindowShouldClose()) {
        UpdateGame(GetFrameTime());
        DrawGame();
    }

    CloseWindow();
    return 0;
}