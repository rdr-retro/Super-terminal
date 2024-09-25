#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <pty.h>
#include <utmp.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>    // Para fork, close, setsid, dup2, execv
#include <sys/types.h> // Para pid_t
#include <sys/wait.h>  // Para waitpid
#include <fcntl.h>     // Para open
#include <errno.h>     // Para manejar errores


#define FRAME_DELAY 100
#define NUM_FRAMES 17
#define IMAGE_WIDTH 78
#define IMAGE_HEIGHT 107
#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 560
#define BOX_MARGIN 8
#define BOX_WIDTH_REDUCTION 16
#define BOX_WIDTH (WINDOW_WIDTH - 2 * BOX_MARGIN)
#define BOX_HEIGHT 118
#define CONTENT_OFFSET_Y 24
#define CONTENT_OFFSET_X 16
#define BOX_VERTICAL_OFFSET 100
#define IMAGE_VERTICAL_OFFSET -5
#define IMAGE_SPACING 0
#define LEFT_BOX_WIDTH (BOX_WIDTH / 3.5)
#define RIGHT_BOX_WIDTH (BOX_WIDTH - LEFT_BOX_WIDTH)

#define COLOR_BACKGROUND_R 9
#define COLOR_BACKGROUND_G 48
#define COLOR_BACKGROUND_B 91
#define COLOR_BOX_R 1
#define COLOR_BOX_G 8
#define COLOR_BOX_B 34
#define COLOR_TEXT_R 255
#define COLOR_TEXT_G 255
#define COLOR_TEXT_B 255

#define MAX_INPUT_LENGTH 256
#define MAX_OUTPUT_LENGTH 1024
#define OUTPUT_BOX_MARGIN 8
#define OUTPUT_BOX_LINES 17  // Número de líneas visibles en la caja de salida
#define OUTPUT_BOX_CHARS_PER_LINE 150  // Número de caracteres por línea en la caja de salida

#define BAR_WIDTH 16
#define BAR_HEIGHT 100
#define BAR_SPACING 16
#define TOP_BOX_MARGIN 8

#define COLOR_CPU_BAR_R 0
#define COLOR_CPU_BAR_G 0
#define COLOR_CPU_BAR_B 0
#define COLOR_RAM_BAR_R 0
#define COLOR_RAM_BAR_G 0
#define COLOR_RAM_BAR_B 0
#define COLOR_BAR_FILL_R 255
#define COLOR_BAR_FILL_G 255
#define COLOR_BAR_FILL_B 255


typedef struct {
    char command[MAX_INPUT_LENGTH];
    char output[MAX_OUTPUT_LENGTH];
    int master_fd;
} CommandData;

int getRealCPUUsage() {
    static long long lastTotalUser, lastTotalUserLow, lastTotalSys, lastTotalIdle;
    long long totalUser, totalUserLow, totalSys, totalIdle, total;

    FILE* file = fopen("/proc/stat", "r");
    if (file == NULL) {
        perror("Error abriendo /proc/stat");
        return 0;
    }

    fscanf(file, "cpu %lld %lld %lld %lld", &totalUser, &totalUserLow, &totalSys, &totalIdle);
    fclose(file);

    if (lastTotalUser == 0 && lastTotalUserLow == 0 && lastTotalSys == 0 && lastTotalIdle == 0) {
        lastTotalUser = totalUser;
        lastTotalUserLow = totalUserLow;
        lastTotalSys = totalSys;
        lastTotalIdle = totalIdle;
        return 0;
    }

    long long diffUser = totalUser - lastTotalUser;
    long long diffUserLow = totalUserLow - lastTotalUserLow;
    long long diffSys = totalSys - lastTotalSys;
    long long diffIdle = totalIdle - lastTotalIdle;

    lastTotalUser = totalUser;
    lastTotalUserLow = totalUserLow;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;

    total = diffUser + diffUserLow + diffSys;
    long long totalTime = total + diffIdle;
    int cpuUsage = (int)((100.0 * total) / totalTime);

    return cpuUsage;
}

int getRealRAMUsage() {
    long totalMemory = 0;
    long freeMemory = 0;
    char buffer[256];

    FILE* file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        perror("Error abriendo /proc/meminfo");
        return 0;
    }

    while (fgets(buffer, sizeof(buffer), file)) {
        if (sscanf(buffer, "MemTotal: %ld kB", &totalMemory) == 1) {
        }
        if (sscanf(buffer, "MemAvailable: %ld kB", &freeMemory) == 1) {
            break;
        }
    }
    fclose(file);

    long usedMemory = totalMemory - freeMemory;
    int ramUsage = (int)((100.0 * usedMemory) / totalMemory);

    return ramUsage;
}


void cleanup(SDL_Texture *textures[], SDL_Renderer *renderer, SDL_Window *window, TTF_Font *font) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (textures[i]) SDL_DestroyTexture(textures[i]);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void* execute_command(void* arg) {
    CommandData* cmdData = (CommandData*)arg;
    int master, slave;
    char *slave_name;
    
    if (openpty(&master, &slave, NULL, NULL, NULL) == -1) {
        snprintf(cmdData->output, MAX_OUTPUT_LENGTH, "Error al abrir pty.");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        snprintf(cmdData->output, MAX_OUTPUT_LENGTH, "Error al crear proceso hijo.");
        return NULL;
    } else if (pid == 0) {
        // Proceso hijo
        close(master);
        setsid();
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if (ioctl(slave, TIOCSCTTY, NULL) == -1) {
            exit(1);
        }
        close(slave);
        
        char *args[] = {"/bin/sh", "-c", cmdData->command, NULL};
        execv("/bin/sh", args);
        exit(1);
    } else {
        // Proceso padre
        close(slave);
        cmdData->master_fd = master;
        
        fd_set readfds;
        struct timeval tv;
        int ret;
        
        cmdData->output[0] = '\0';
        while (1) {
            FD_ZERO(&readfds);
            FD_SET(master, &readfds);
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            
            ret = select(master + 1, &readfds, NULL, NULL, &tv);
            if (ret > 0) {
                char buffer[1024];
                int n = read(master, buffer, sizeof(buffer) - 1);
                if (n <= 0) break;
                buffer[n] = '\0';
                strncat(cmdData->output, buffer, MAX_OUTPUT_LENGTH - strlen(cmdData->output) - 1);
            } else if (ret == 0) {
                // Timeout, verificar si el proceso hijo ha terminado
                int status;
                if (waitpid(pid, &status, WNOHANG) != 0) break;
            } else {
                break;
            }
        }
        
        close(master);
    }
    
    return NULL;
}

void wrapText(const char* input, char output[OUTPUT_BOX_LINES][OUTPUT_BOX_CHARS_PER_LINE + 1]) {
    int line = 0;
    int charCount = 0;
    const char* word = input;

    for (int i = 0; i < OUTPUT_BOX_LINES; i++) {
        memset(output[i], 0, OUTPUT_BOX_CHARS_PER_LINE + 1);
    }

    while (*word != '\0' && line < OUTPUT_BOX_LINES) {
        if (charCount == OUTPUT_BOX_CHARS_PER_LINE || *word == '\n') {
            line++;
            charCount = 0;
            if (*word == '\n') word++;
            continue;
        }

        if (line < OUTPUT_BOX_LINES) {
            output[line][charCount++] = *word++;
        }
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "No se pudo inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "No se pudo inicializar SDL_image: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "No se pudo inicializar SDL_ttf: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Terminal",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "No se pudo crear la ventana: %s\n", SDL_GetError());
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        fprintf(stderr, "No se pudo crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font *font = TTF_OpenFont("fonts/Hack-Regular.ttf", 9);
    if (font == NULL) {
        fprintf(stderr, "No se pudo cargar la fuente: %s\n", TTF_GetError());
        cleanup(NULL, renderer, window, NULL);
        return 1;
    }

    char inputText[MAX_INPUT_LENGTH] = "";
    char outputText[MAX_OUTPUT_LENGTH] = "Output: ";
    char wrappedOutput[OUTPUT_BOX_LINES][OUTPUT_BOX_CHARS_PER_LINE + 1];

    SDL_StartTextInput();

    SDL_Texture *textures[NUM_FRAMES];
    char filename[20];
    for (int i = 0; i < NUM_FRAMES; i++) {
        sprintf(filename, "images/%04d.png", 124 + i);
        textures[i] = IMG_LoadTexture(renderer, filename);
        if (textures[i] == NULL) {
            fprintf(stderr, "Error al cargar imagen %s: %s\n", filename, IMG_GetError());
            cleanup(textures, renderer, window, font);
            return 1;
        }
    }

    SDL_Color textColor = {COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255};
    SDL_Surface *textSurface1 = TTF_RenderText_Blended(font, "Super terminal de comandos", textColor);
    SDL_Surface *textSurface2 = TTF_RenderText_Blended(font, "version 1.0", textColor);
    SDL_Texture *textTexture1 = SDL_CreateTextureFromSurface(renderer, textSurface1);
    SDL_Texture *textTexture2 = SDL_CreateTextureFromSurface(renderer, textSurface2);
    SDL_FreeSurface(textSurface1);
    SDL_FreeSurface(textSurface2);

    int textWidth1, textHeight1, textWidth2, textHeight2;
    SDL_QueryTexture(textTexture1, NULL, NULL, &textWidth1, &textHeight1);
    SDL_QueryTexture(textTexture2, NULL, NULL, &textWidth2, &textHeight2);

    int frame = 0;
    int running = 1;
    SDL_Event event;
    Uint32 frame_start;
    Uint32 frame_time;
    Uint32 lastUpdate = SDL_GetTicks();

    int cpuBarX = LEFT_BOX_WIDTH + BOX_MARGIN + CONTENT_OFFSET_X - 145;
    int cpuBarY = BOX_VERTICAL_OFFSET + TOP_BOX_MARGIN + 1.5;
    int ramBarX = cpuBarX + BAR_WIDTH + BAR_SPACING;
    int ramBarY = cpuBarY;

    int cpuUsage = 0;
    int ramUsage = 0;

    int scrollOffset = 0;
    pthread_t commandThread;
    CommandData cmdData;

    int caretVisible = 1;  // Variable para mostrar/ocultar el caret
    Uint32 caretTimer = SDL_GetTicks();  // Timer para el caret

  // Define un cursor de escritura (puedes usar una imagen o el cursor predeterminado)
SDL_Cursor* textCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM); // Cursor de escritura
SDL_Cursor* defaultCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW); // Cursor por defecto

// En el bucle principal, justo después de procesar eventos:
while (running) {
    frame_start = SDL_GetTicks();

    // Cambiar el cursor dependiendo de la posición del mouse
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    
    // Definir la caja de texto (ajusta la posición y el tamaño según tu código)
    SDL_Rect inputBox = {BOX_MARGIN + LEFT_BOX_WIDTH + 8, BOX_VERTICAL_OFFSET + 8, 
                         WINDOW_WIDTH - (BOX_MARGIN + LEFT_BOX_WIDTH + 16), 30}; // Suponiendo altura de 30

    // Cambiar el cursor si el mouse está sobre la caja de texto
    if (mouseX >= inputBox.x && mouseX <= inputBox.x + inputBox.w &&
        mouseY >= inputBox.y && mouseY <= inputBox.y + inputBox.h) {
        SDL_SetCursor(textCursor); // Cambiar a cursor de escritura
    } else {
        SDL_SetCursor(defaultCursor); // Cambiar al cursor por defecto
    }

    while (SDL_PollEvent(&event)) {
        if (cmdData.master_fd > 0) {
            close(cmdData.master_fd);
        }
        if (event.type == SDL_QUIT) {
            running = 0;
        } else if (event.type == SDL_TEXTINPUT) {
            if (strlen(inputText) + strlen(event.text.text) < MAX_INPUT_LENGTH - 1) {
                strcat(inputText, event.text.text);
            }
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_BACKSPACE && strlen(inputText) > 0) {
                inputText[strlen(inputText) - 1] = '\0';
            } else if (event.key.keysym.sym == SDLK_RETURN && strlen(inputText) > 0) {
                snprintf(cmdData.command, MAX_INPUT_LENGTH, "%s", inputText);
                pthread_create(&commandThread, NULL, execute_command, (void*)&cmdData);
                pthread_detach(commandThread);

                // Reiniciar el texto de entrada y el caret
                inputText[0] = '\0';
                caretVisible = 1;  // Asegúrate de que el caret esté visible de nuevo
                caretTimer = SDL_GetTicks();  // Reiniciar el temporizador del caret
            } else if (event.key.keysym.sym == SDLK_UP) {
                scrollOffset -= 2;
                if (scrollOffset < 0) scrollOffset = 0;
            } else if (event.key.keysym.sym == SDLK_DOWN) {
                scrollOffset += 2;
            }
        } else if (event.type == SDL_MOUSEWHEEL) {
            scrollOffset += event.wheel.y * 10;  // Ajusta la sensibilidad aquí
            if (scrollOffset < 0) scrollOffset = 0;
        }
    }

    // Actualiza el uso de CPU y RAM solo si ha pasado el intervalo de actualización
    static Uint32 lastUpdate = 0; // Variable estática para mantener el estado entre iteraciones
    if (SDL_GetTicks() - lastUpdate >= 500) {
        cpuUsage = getRealCPUUsage(); // Asegúrate de que esta función esté implementada correctamente
        ramUsage = getRealRAMUsage(); // Asegúrate de que esta función esté implementada correctamente
        lastUpdate = SDL_GetTicks();
    }

    // Alternar la visibilidad del caret cada 500 ms
    static Uint32 caretTimer = 0; // Variable estática para mantener el estado entre iteraciones
    if (SDL_GetTicks() - caretTimer >= 500) {
        caretVisible = !caretVisible;
        caretTimer = SDL_GetTicks();
    }

        SDL_SetRenderDrawColor(renderer, COLOR_BACKGROUND_R, COLOR_BACKGROUND_G, COLOR_BACKGROUND_B, 255);
        SDL_RenderClear(renderer);

        int leftMargin = BOX_MARGIN;

        SDL_Rect textRect1 = {leftMargin, 16, textWidth1, textHeight1};
        SDL_RenderCopy(renderer, textTexture1, NULL, &textRect1);

        SDL_Rect textRect2 = {leftMargin, 16 + textHeight1 + 8, textWidth2, textHeight2};
        SDL_RenderCopy(renderer, textTexture2, NULL, &textRect2);

        SDL_Rect caja2 = {BOX_MARGIN, BOX_VERTICAL_OFFSET + BOX_HEIGHT + OUTPUT_BOX_MARGIN, WINDOW_WIDTH - 2 * BOX_MARGIN, WINDOW_HEIGHT - (BOX_VERTICAL_OFFSET + BOX_HEIGHT + OUTPUT_BOX_MARGIN)};
        SDL_SetRenderDrawColor(renderer, 9, 48, 91, 255);
        SDL_RenderFillRect(renderer, &caja2);

        SDL_Rect leftBox = {BOX_MARGIN, BOX_VERTICAL_OFFSET, LEFT_BOX_WIDTH, BOX_HEIGHT};
        SDL_SetRenderDrawColor(renderer, COLOR_BOX_R, COLOR_BOX_G, COLOR_BOX_B, 255);
        SDL_RenderFillRect(renderer, &leftBox);
        SDL_SetRenderDrawColor(renderer, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255);
        SDL_RenderDrawRect(renderer, &leftBox);

        SDL_Rect rightBox = {BOX_MARGIN + LEFT_BOX_WIDTH, BOX_VERTICAL_OFFSET, RIGHT_BOX_WIDTH, BOX_HEIGHT};
        SDL_SetRenderDrawColor(renderer, COLOR_BOX_R, COLOR_BOX_G, COLOR_BOX_B, 255);
        SDL_RenderFillRect(renderer, &rightBox);
        SDL_SetRenderDrawColor(renderer, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255);
        SDL_RenderDrawRect(renderer, &rightBox);

        SDL_SetRenderDrawColor(renderer, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255);
        SDL_RenderDrawLine(renderer, BOX_MARGIN + LEFT_BOX_WIDTH, BOX_VERTICAL_OFFSET, BOX_MARGIN + LEFT_BOX_WIDTH, BOX_VERTICAL_OFFSET + BOX_HEIGHT);

        SDL_Rect imageRect = {
            BOX_MARGIN + LEFT_BOX_WIDTH + CONTENT_OFFSET_X - 99,
            BOX_VERTICAL_OFFSET + IMAGE_VERTICAL_OFFSET + 11,
            IMAGE_WIDTH, 
            IMAGE_HEIGHT
        };
        SDL_RenderCopy(renderer, textures[frame], NULL, &imageRect);

        SDL_Surface *inputSurface = TTF_RenderText_Blended(font, inputText, textColor);
        SDL_Texture *inputTexture = SDL_CreateTextureFromSurface(renderer, inputSurface);
        SDL_FreeSurface(inputSurface);

        int inputWidth, inputHeight;
        SDL_QueryTexture(inputTexture, NULL, NULL, &inputWidth, &inputHeight);
        SDL_Rect inputRect = {BOX_MARGIN + LEFT_BOX_WIDTH + 8, BOX_VERTICAL_OFFSET + 8, inputWidth, inputHeight};
        SDL_RenderCopy(renderer, inputTexture, NULL, &inputRect);
        SDL_DestroyTexture(inputTexture);

       // Renderizar el caret parpadeante
    if (caretVisible) {
    SDL_Rect caretRect = {inputRect.x + inputRect.w + 2, inputRect.y, 10, 14}; // Altura fija de 14
    SDL_SetRenderDrawColor(renderer, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 255);
    SDL_RenderFillRect(renderer, &caretRect);
}


        wrapText(cmdData.output, wrappedOutput);

        for (int i = 0; i < OUTPUT_BOX_LINES; i++) {
            if (strlen(wrappedOutput[i]) > 0) {
                SDL_Surface *outputSurface = TTF_RenderText_Blended(font, wrappedOutput[i], textColor);
                SDL_Texture *outputTexture = SDL_CreateTextureFromSurface(renderer, outputSurface);
                SDL_FreeSurface(outputSurface);

                int outputWidth, outputHeight;
                SDL_QueryTexture(outputTexture, NULL, NULL, &outputWidth, &outputHeight);
                SDL_Rect outputRect = {
                    caja2.x + BOX_MARGIN,
                    caja2.y + (i * (outputHeight + 2)) - scrollOffset,  // Aplicar desplazamiento
                    outputWidth, outputHeight
                };

                // Limitar el desplazamiento vertical para no mostrar contenido fuera de la caja
                if (outputRect.y + outputRect.h > caja2.y + caja2.h) {
                    outputRect.h = caja2.y + caja2.h - outputRect.y;
                }
                if (outputRect.y < caja2.y) {
                    outputRect.h -= caja2.y - outputRect.y;
                    outputRect.y = caja2.y;
                }

                if (outputRect.h > 0) {
                    SDL_RenderCopy(renderer, outputTexture, NULL, &outputRect);
                }
                SDL_DestroyTexture(outputTexture);
            }
        }

        SDL_Rect cpuFillBar = {cpuBarX, cpuBarY + BAR_HEIGHT - ((100 - cpuUsage) * BAR_HEIGHT / 100), BAR_WIDTH, (100 - cpuUsage) * BAR_HEIGHT / 100};
        SDL_SetRenderDrawColor(renderer, COLOR_BAR_FILL_R, COLOR_BAR_FILL_G, COLOR_BAR_FILL_B, 255);
        SDL_RenderFillRect(renderer, &cpuFillBar);

        SDL_Rect ramFillBar = {ramBarX, ramBarY + BAR_HEIGHT - ((100 - ramUsage) * BAR_HEIGHT / 100), BAR_WIDTH, (100 - ramUsage) * BAR_HEIGHT / 100};
        SDL_SetRenderDrawColor(renderer, COLOR_BAR_FILL_R, COLOR_BAR_FILL_G, COLOR_BAR_FILL_B, 255);
        SDL_RenderFillRect(renderer, &ramFillBar);

        SDL_RenderPresent(renderer);

        frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }

        frame = (frame + 1) % NUM_FRAMES;
    }

    SDL_StopTextInput();
    cleanup(textures, renderer, window, font);

    return 0;
}