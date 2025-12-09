#include "raylib.h"
#include <string>
#include <cmath> // Do fizyki rzutu
#include <cstdio> // Do TextFormat/sprintf
using namespace std;
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
    MINIGAME_FLANKI,
    MINIGAME_EGZAMIN
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
enum PhoneState {
    PHONE_CLOSED, // Telefon w kieszeni
    MENU1,         // Wybór aplikacji
    PIN,          // Ekran logowania
    ACCOUNT       // Saldo konta
};

// --- STRUKTURY ---
struct Player {
    Vector2 position;
    float speed;
    int level;
    Color color;
    float hunger;
    float money;
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

struct Phone {
    PhoneState state;  // Co telefon robi teraz
    string correctPin;    // Prawidłowe hasło 
    int selectedIcon;  // Wybrana ikona w menu
    string inputPin; // Wprowadzany PIN
    bool showError; // Czy pokazać błąd przy PIN-ie
};

// Tworzymy telefon: { Stan_Początkowy, PIN }
Phone phone = { PHONE_CLOSED, "0000", 0, "", false };
// --- ZMIENNE GLOBALNE ---
const int screenWidth = 800;
const int screenHeight = 600;

Player player = { { 400, 300 }, 200.0f, 1, BLUE, 100.0f, 20.0f};
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
    // Telefon
    if(IsKeyPressed(KEY_UP)){
        if(phone.state == PHONE_CLOSED){
            phone.state = MENU1;
        }
    }
    if(IsKeyPressed(KEY_BACKSPACE)){
        if(phone.state == MENU1 || phone.state == PIN || phone.state == ACCOUNT){
            phone.state = PHONE_CLOSED;
        }
    }
    if(phone.state == MENU1 || phone.state == PIN || phone.state == ACCOUNT){
        if(IsKeyPressed(KEY_Q)){
            if(phone.state == ACCOUNT) phone.state = MENU1; 
        }
        
    }
    if(phone.state == MENU1 && IsKeyPressed(KEY_ENTER) && phone.selectedIcon == 0){
        phone.state = PIN; // Przejście do ekranu PIN
    }
    else if(phone.state == PIN){
        // 1. Obsługa wpisywania tekstu (cyfr)
        int key = GetCharPressed();

        while (key > 0) {
            // Sprawdzamy kod ASCII: od 48 ('0') do 57 ('9')
            // ORAZ czy wpisaliśmy mniej niż 4 znaki
            if ((key >= 48) && (key <= 57) && (phone.inputPin.length() < 4)) {
                phone.inputPin += (char)key; // Dodajemy znak do naszego stringa
            }
            key = GetCharPressed(); // Pobierz kolejny znak (dla szybkiego pisania)
        }

        // 2. Obsługa kasowania (Backspace)
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (phone.inputPin.length() > 0) {
                // Usuwa ostatni znak z napisu
                phone.inputPin.pop_back(); 
            }
        }

        // 3. Zatwierdzanie PIN-u ENTEREM
        if (IsKeyPressed(KEY_ENTER)) {
             // Tutaj zaraz dopiszemy sprawdzanie hasła!
             if(phone.inputPin == phone.correctPin){
                phone.state = ACCOUNT; // Poprawny PIN, przejście do konta
                phone.inputPin = ""; // Czyszczenie wprowadzanego PIN-u
                phone.showError = false; // Nie pokazujemy błędu
            } else {
                phone.showError = true; // Błędny PIN, pokazujemy błąd
                phone.inputPin = ""; // Czyszczenie wprowadzanego PIN-u
            }
             
        }
       }

    
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
        // Logika dla ekranu Egzaminu
    if (currentScreen == MINIGAME_EGZAMIN) {
        // Jeśli gracz wciśnie klawisz "4" (poprawna odpowiedź)
        if (IsKeyPressed(KEY_FOUR)) {
            player.money += 50;        // 1. Dodaj 50 zł stypendium
            currentScreen = GAMEPLAY;  // 2. Wróć do chodzenia
              
        } return; //zatrzymuje ruch gracza podczas egzaminu
    }
    // --- LOGIKA NORMALNA (CHODZENIE) ---
    
    // Ruch
    if (IsKeyDown(KEY_W)){ 
        player.position.y -= player.speed * dt;
        player.hunger -= 0.5f * dt; // Zmniejszanie głodu przy ruchu
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

        Rectangle kebabRect = { 100, 100, 60, 40 }; // Kebap w parku
        if (CheckCollisionRecs(playerRect, kebabRect)){
            if(IsKeyPressed(KEY_E)){
            if(player.money >= 15.0f){
                player.money -= 15.0f;
                player.hunger += 30.0f; // Zwiększa głód o 30
                if (player.hunger > 100.0f) player.hunger = 100.0f; // Maksymalny głód 100
            } else if (player.money < 15.0f){
                DrawText("NIE STAĆ CIĘ BIEDAKU!", 100, 110, 10, RED);
            }
            
        }
    }

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
        // Stolik egzaminacyjny
        Rectangle examRect = {350, 300, 100, 60}; // Stolik egzaminacyjny
            if (CheckCollisionRecs(playerRect, examRect) && IsKeyPressed(KEY_E)) {
                currentScreen = MINIGAME_EGZAMIN;
                
            }
    }
}

// --- DRAW (RYSOWANIE) ---
void DrawGame() {
    BeginDrawing();
    ClearBackground(BLACK);
    if (phone.state == MENU1) {
        // 1. Obudowa telefonu (Prawy dolny róg)
        DrawRectangle(560, 180, 220, 400, DARKGRAY);
        DrawRectangleLines(560, 180, 220, 400, BLACK); // Opcjonalna czarna ramka obudowy

        // 2. Ikona Banku niebieski kwadrat)
        DrawRectangle(590, 210, 50, 50, BLUE);
        DrawText("BANK", 595, 225, 10, WHITE);

        // 3. Podświetlenie (Jeśli wybrana jest ikona nr 0)
        if (phone.selectedIcon == 0) {
            // Czerwona ramka DOKŁADNIE w tym samym miejscu co ikona
            DrawRectangleLines(590, 210, 50, 50, RED);
        }
    } else if(phone.state == PIN){
        // Ekran PIN
        DrawRectangle(560, 180, 220, 400, LIGHTGRAY);
        DrawRectangleLines(560, 180, 220, 400, BLACK); // Ramka ekranu

        DrawText("WPROWADZ PIN:", 580, 200, 15, BLACK);
        // Rysujemy wprowadzony PIN (gwiazdki dla bezpieczeństwa)
        DrawText(phone.inputPin.c_str(), 600, 250, 20, BLACK);
        // warunek do wyswietlania bledu
        if(phone.showError == true){
            DrawText("BLEDNY PIN!", 580, 300, 15, RED);
        }
    } else if(phone.state == ACCOUNT){
        // 1. Tło (może białe dla banku?)
    DrawRectangle(560, 180, 220, 400, RAYWHITE);
    DrawRectangleLines(560, 180, 220, 400, BLACK);

    DrawText("SALDO KONTA:", 580, 220, 10, DARKGRAY);
    // Wyświetlanie pieniędzy gracza
    DrawText(TextFormat("%.2f zl", player.money), 590, 240, 20, GREEN);

        // Instrukcja powrotu (żeby gracz wiedział co kliknąć)
        DrawText("[Q] Wroc", 630, 550, 10, LIGHTGRAY);
    }

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

    } else if(currentScreen == MINIGAME_EGZAMIN){
        ClearBackground(LIGHTGRAY);
        DrawText("EGZAMIN: Ile to jest 2 + 2?", 100, 100, 20, BLACK);
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
            //Obiekt Kebap
            DrawRectangle(100, 100, 60, 40, YELLOW);
            DrawText("Kebap", 110, 110, 10, BLACK);
        }
        else if (currentLocation == KAMPUS) {
            ClearBackground(LIGHTGRAY);
            DrawText("KAMPUS", 20, 20, 20, DARKBLUE);
            DrawRectangle(0, screenHeight/2 - 50, 20, 100, DARKGREEN); // Do parku

            // Stolik egzaminacyjny
            DrawRectangle(350, 300, 100, 60, PURPLE);
            DrawText("EGZAMIN", 365, 320, 10, WHITE);
        }

        // Rysuj gracza
        DrawRectangle((int)player.position.x, (int)player.position.y, 40, 40, player.color);
        
        // --- NOWOŚĆ: RYSOWANIE PASKA LEVELOWANIA RÓWNIEŻ W ŚWIECIE GRY ---
        // (Jeśli chcesz, żeby pasek było widać też jak chodzisz, zostaw to. 
        //  Jeśli ma być tylko we flankach, usuń poniższą linijkę)
        stats.DrawHUD();
        // 1. Ustalanie koloru w zależności od głodu
        Color hungerColor = GREEN;
        if (player.hunger < 50.0f) hungerColor = ORANGE;
        if (player.hunger < 20.0f) hungerColor = RED;

        // 2. Wymiary i pozycja (pod paskiem poziomu)
        float barWidth = 220.0f;
        float barHeight = 30.0f;
        float x = GetScreenWidth() - barWidth - 20; // Ta sama pozycja X co poziom
        float y = 60.0f; // Nieco niżej niż pasek poziomu

        // 3. Rysowanie tła (szare)
        DrawRectangle(x, y, barWidth, barHeight, Fade(BLACK, 0.6f));

        // 4. Rysowanie paska głodu (zmieniający się kolor)
        float hungerPercent = player.hunger / 100.0f; // Obliczamy procent (0.0 do 1.0)
        DrawRectangle(x, y, barWidth * hungerPercent, barHeight, hungerColor);

        // 5. Ramka i napis
        DrawRectangleLines(x, y, barWidth, barHeight, WHITE);
        DrawText("GLOD", x + 10, y + 8, 10, WHITE);
        DrawText(TextFormat("%d%%", (int)player.hunger), x + barWidth - 40, y + 8, 10, WHITE);

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