#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp> // Para manejo de reloj, tiempo y eventos
#include <opencv2/opencv.hpp> // Para capturar imagen desde cámara con OpenCV
#include <iostream>
#include <string>
#include <filesystem> // Para manejar directorios y archivos con std::filesystem
#include <fstream>    // Para manejar archivos (leer y escribir)
#include <sstream>    // Para manejar flujos de strings, útil en cifrado y lectura
#include <thread>     // En caso de que quieras usar hilos (no usado aquí directamente)
#include <vector>     // Para manejar listas dinámicas
#include <algorithm>  // Para funciones estándar como std::minmax
#include <cmath>      // Funciones matemáticas
#include <array>      // Para buffer en popen
#include "xor_lib.hpp" // Biblioteca externa propia para gestión de cámara y generación de claves

namespace fs = std::filesystem;

// ---------------------------------------------------------------
// Función para cifrar o descifrar texto con XOR y clave (simétrica)
// ---------------------------------------------------------------
std::string cifrarXOR(const std::string& texto, const std::string& clave) {
    std::string resultado = texto;
    for (size_t i = 0; i < texto.size(); ++i)
        resultado[i] ^= clave[i % clave.size()];
    return resultado;
}

// ---------------------------------------------------------------
// Función para listar todos los usuarios (carpetas en "Capsulas")
// ---------------------------------------------------------------
std::vector<std::string> listarUsuarios() {
    std::vector<std::string> usuarios;
    fs::path Capsulas("Capsulas");
    if (fs::exists(Capsulas) && fs::is_directory(Capsulas)) {
        for (const auto& entry : fs::directory_iterator(Capsulas)) {
            if (entry.is_directory())
                usuarios.push_back(entry.path().filename().string());
        }
    }
    return usuarios;
}

// ---------------------------------------------------------------
// Estructura para guardar una entrada de la Capsula servicio, usuario y clave
// ---------------------------------------------------------------
struct EntradaCapsula {
    std::string servicio;
    std::string usuario;
    std::string clave;
};

// ---------------------------------------------------------------
// Leer entradas descifradas de archivo .dat
// ---------------------------------------------------------------
std::vector<EntradaCapsula> leerEntradasCapsula(const std::string& usuario, const std::string& clave) {
    std::vector<EntradaCapsula> entradas;
    fs::path archivo = fs::path("Capsulas") / usuario / (usuario + ".dat");
    std::ifstream dat(archivo, std::ios::binary);
    if (!dat) return entradas;
    std::ostringstream ss;
    ss << dat.rdbuf();
    std::string cifrado = ss.str();
    std::string descifrado = cifrarXOR(cifrado, clave);
    std::istringstream iss(descifrado);
    std::string linea;
    bool header = true;
    while (std::getline(iss, linea)) {
        if (header) { header = false; continue; }
        if (linea.empty()) continue;
        size_t pos1 = linea.find('|');
        size_t pos2 = linea.rfind('|');
        if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) {
            entradas.push_back({
                linea.substr(0, pos1),
                linea.substr(pos1 + 1, pos2 - pos1 - 1),
                linea.substr(pos2 + 1)
            });
        }
    }
    return entradas;
}

// ---------------------------------------------------------------
// Guardar entradas cifradas en archivo .dat
// ---------------------------------------------------------------
void guardarEntradasCapsula(const std::string& usuario, const std::string& clave, const std::vector<EntradaCapsula>& entradas) {
    fs::path archivo = fs::path("Capsulas") / usuario / (usuario + ".dat");
    std::ostringstream ss;
    ss << "Bienvenido a tu Capsula, " << usuario << "!\n";
    for (const auto& entrada : entradas) {
        ss << entrada.servicio << "|" << entrada.usuario << "|" << entrada.clave << "\n";
    }
    std::string cifrado = cifrarXOR(ss.str(), clave);
    std::ofstream dat(archivo, std::ios::binary | std::ios::trunc);
    dat.write(cifrado.c_str(), cifrado.size());
    dat.close();
}

// ---------------------------------------------------------------
// FUNCIONES CSV COMPLETAS Y ROBUSTAS CON SOPORTE DE CAMPOS CON COMAS Y COMILLAS
// ---------------------------------------------------------------

// Escape CSV: dobla comillas y encierra en comillas
std::string escapeCSV(const std::string& campo) {
    std::string result = "\"";
    for (char c : campo) {
        if (c == '"') result += "\"\"";
        else result += c;
    }
    result += "\"";
    return result;
}

// Importa CSV robusto con parseo correcto de campos con comillas y comas internas
bool importarCapsulaDesdeCSV(const std::string& rutaCSV, std::vector<EntradaCapsula>& entradas) {
    std::ifstream archivo(rutaCSV);
    if (!archivo) return false;

    std::string linea;
    bool primeraLinea = true;

    while (std::getline(archivo, linea)) {
        if (primeraLinea) { primeraLinea = false; continue; }
        if (linea.empty()) continue;

        std::vector<std::string> campos;
        std::string campo;
        bool dentroComillas = false;

        for (size_t i = 0; i < linea.size(); ++i) {
            char c = linea[i];
            if (c == '"') {
                if (dentroComillas && i + 1 < linea.size() && linea[i + 1] == '"') {
                    campo += '"'; // Doble comilla -> una comilla literal
                    ++i;
                } else {
                    dentroComillas = !dentroComillas;
                }
            } else if (c == ',' && !dentroComillas) {
                campos.push_back(campo);
                campo.clear();
            } else {
                campo += c;
            }
        }
        campos.push_back(campo);

        if (campos.size() != 3) continue;

        // Trim espacios
        for (auto& c : campos) {
            while (!c.empty() && (c.front() == ' ' || c.front() == '\t')) c.erase(0, 1);
            while (!c.empty() && (c.back() == ' ' || c.back() == '\t')) c.pop_back();
        }

        if (!campos[0].empty() && !campos[1].empty() && !campos[2].empty()) {
            entradas.push_back({campos[0], campos[1], campos[2]});
        }
    }
    archivo.close();
    return true;
}

// ---------------------------------------------------------------
// FUNCIONES DIÁLOGOS GRÁFICOS UBUNTU CON ZENITY PARA CSV
// ---------------------------------------------------------------

std::string seleccionarCarpetaZenity() {
    std::array<char, 128> buffer;
    std::string result;

    FILE* pipe = popen("zenity --file-selection --directory --title=\"Selecciona carpeta para exportar CSV\"", "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);

    if (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    return result;
}

std::string seleccionarArchivoZenity() {
    std::array<char, 128> buffer;
    std::string result;

    FILE* pipe = popen("zenity --file-selection --title=\"Selecciona archivo CSV para importar\" --file-filter=\"*.csv\"", "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);

    if (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    return result;
}

// ---------------------------------------------------------------
// Resto estructuras y constantes originales
// ---------------------------------------------------------------
enum class Estado {
    MenuPrincipal,
    Registro,
    RegistroMostrarClave,
    LoginSeleccionUsuario,
    LoginPedirPassword,
    Capsula,
    CapsulaAñadir,
    ErrorCamara
};

bool esAlfanumerico(char c) {
    return std::isalnum(static_cast<unsigned char>(c));
}

const sf::Color fondoPrincipal(45, 45, 45);
const sf::Color grisMedio(41, 49, 58);
const sf::Color azulLinaje(0, 239, 203);
const sf::Color azulClaro(120, 126, 127);
const sf::Color textoClaro(255, 255, 255);
const sf::Color textoSecundario(120, 126, 127);
const sf::Color errorColor(255, 80, 80);
const sf::Color fondoFilaPar(41, 49, 58);
const sf::Color fondoFilaImpar(32, 38, 38);
const sf::Color filaSeleccionada = azulLinaje;
const sf::Time CURSOR_BLINK_INTERVAL = sf::milliseconds(500);

struct CampoTexto {
    std::string texto;
    int cursorPos = 0;
    int seleccionInicio = -1;
    int seleccionFin = -1;
    bool activo = false;
    sf::Clock relojCursor;
    bool mostrarCursor = true;
    sf::FloatRect rect;

    CampoTexto() = default;
    CampoTexto(const sf::FloatRect& r) : rect(r) {}

    void setTexto(const std::string& t) {
        texto = t;
        cursorPos = (int)texto.size();
        clearSeleccion();
    }
    void clearSeleccion() {
        seleccionInicio = -1;
        seleccionFin = -1;
    }
    bool tieneSeleccion() const {
        return seleccionInicio != -1 && seleccionFin != -1 && seleccionInicio != seleccionFin;
    }
    std::pair<int,int> rangoSeleccion() const {
        if (!tieneSeleccion()) return {0,0};
        return std::minmax(seleccionInicio, seleccionFin);
    }
    std::string textoSeleccionado() const {
        if (!tieneSeleccion()) return "";
        auto [ini, fin] = rangoSeleccion();
        return texto.substr(ini, fin - ini);
    }
    void borrarSeleccion() {
        if (!tieneSeleccion()) return;
        auto [ini, fin] = rangoSeleccion();
        texto.erase(ini, fin - ini);
        cursorPos = ini;
        clearSeleccion();
    }
    void insertarTexto(const std::string& nuevoTexto) {
        borrarSeleccion();
        texto.insert(cursorPos, nuevoTexto);
        cursorPos += (int)nuevoTexto.size();
    }
    void moverCursor(int pos) {
        cursorPos = std::max(0, std::min(pos, (int)texto.size()));
    }
    void updateCursorBlink() {
        if (relojCursor.getElapsedTime() >= CURSOR_BLINK_INTERVAL) {
            mostrarCursor = !mostrarCursor;
            relojCursor.restart();
        }
    }
    int getCursorPosDesdeX(float mouseX, const sf::Font& font, unsigned int fontSize) {
        float xRel = mouseX - rect.left - 5;
        if (xRel <= 0) return 0;
        int pos = 0;
        float anchoAcum = 0.f;
        for (size_t i = 0; i < texto.size(); i++) {
            sf::Glyph glyph = font.getGlyph(texto[i], fontSize, false);
            float anchoChar = glyph.advance;
            if (anchoAcum + anchoChar / 2.f >= xRel) {
                pos = (int)i;
                break;
            }
            anchoAcum += anchoChar;
            pos = (int)(i+1);
        }
        return pos;
    }
    void dibujar(sf::RenderWindow& window, const sf::Font& font, unsigned int fontSize,
                 const sf::Color& textoColor, const sf::Color& seleccionColor) {
        sf::RectangleShape fondo(sf::Vector2f(rect.width, rect.height));
        fondo.setPosition(rect.left, rect.top);
        fondo.setFillColor(activo ? azulLinaje : grisMedio);
        window.draw(fondo);

        sf::Text textoRender;
        textoRender.setFont(font);
        textoRender.setCharacterSize(fontSize);

        if (!tieneSeleccion()) {
            textoRender.setString(texto);
            textoRender.setFillColor(textoColor);
            textoRender.setPosition(rect.left + 5, rect.top + (rect.height - fontSize) / 2.f - 5);
            window.draw(textoRender);
        } else {
            auto [ini, fin] = rangoSeleccion();

            std::string antes = texto.substr(0, ini);
            textoRender.setString(antes);
            textoRender.setFillColor(textoColor);
            textoRender.setPosition(rect.left + 5, rect.top + (rect.height - fontSize) / 2.f - 5);
            window.draw(textoRender);

            float xSel = textoRender.findCharacterPos((unsigned int)antes.size()).x;

            std::string sel = texto.substr(ini, fin - ini);
            textoRender.setString(sel);

            sf::RectangleShape fondoSel(sf::Vector2f(textoRender.getLocalBounds().width, rect.height));
            fondoSel.setPosition(xSel, rect.top);
            sf::Color selColor = seleccionColor;
            selColor.a = 120;
            fondoSel.setFillColor(selColor);
            window.draw(fondoSel);

            textoRender.setFillColor(textoClaro);
            textoRender.setPosition(xSel, rect.top + (rect.height - fontSize) / 2.f - 5);
            window.draw(textoRender);

            std::string despues = texto.substr(fin);
            textoRender.setString(despues);
            textoRender.setFillColor(textoColor);
            float xDespues = fondoSel.getPosition().x + fondoSel.getSize().x;
            textoRender.setPosition(xDespues, rect.top + (rect.height - fontSize) / 2.f - 5);
            window.draw(textoRender);
        }

        if (activo && mostrarCursor) {
            sf::Vector2f posCursor = textoRender.findCharacterPos(cursorPos);
            sf::RectangleShape cursorShape(sf::Vector2f(2.f, rect.height * 0.8f));
            cursorShape.setFillColor(textoClaro);
            cursorShape.setPosition(posCursor.x, rect.top + rect.height * 0.1f);
            window.draw(cursorShape);
        }
    }
};

// ---------------------------------------------------------------
// Función main() con integración completa CSV y GUI con diálogos zenity
// ---------------------------------------------------------------
int main() {
    sf::RenderWindow window(sf::VideoMode(800, 700), "Raiz - Gestor de Contraseñas (CSV con diálogo gráfico)");

    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("Product Sans Regular.ttf")) {
        std::cerr << "No se pudo cargar la fuente.\n";
        return -1;
    }

    sf::Texture texNoCam, texSiCam;
    if (!texNoCam.loadFromFile("nocamera.png") || !texSiCam.loadFromFile("sicamera.png")) {
        std::cerr << "Error cargando imágenes de cámara.\n";
        return -1;
    }
    texNoCam.setSmooth(true);
    texSiCam.setSmooth(true);

    sf::Texture texTree;
    if (!texTree.loadFromFile("tree.png")) {
        std::cerr << "Error cargando imagen tree.png\n";
    } else {
        texTree.setSmooth(true);
    }
    sf::Sprite spriteTree;
    spriteTree.setTexture(texTree);

    sf::Sprite spriteCam;
    spriteCam.setScale(0.28f, 0.28f);

    xor_lib::Generator gen;
    bool camaraDisponible = false;
    try {
        camaraDisponible = gen.hayCamaraDisponible();
    } catch (...) {
        camaraDisponible = false;
    }
    spriteCam.setTexture(camaraDisponible ? texSiCam : texNoCam);

    Estado estado = camaraDisponible ? Estado::MenuPrincipal : Estado::ErrorCamara;

    std::string mensajeSistema, usuarioActual, claveActual;
    std::string registroMensaje, claveGeneradaRegistro;

    CampoTexto campoRegistroUsuario(sf::FloatRect(200.f, 190.f, 400.f, 45.f));
    CampoTexto campoPassword(sf::FloatRect(200.f, 230.f, 400.f, 60.f));
    CampoTexto campoNuevoServicio(sf::FloatRect(200.f, 180.f, 400.f, 40.f));
    CampoTexto campoNuevoUsuario(sf::FloatRect(200.f, 250.f, 400.f, 40.f));
    CampoTexto campoNuevaClave(sf::FloatRect(200.f, 320.f, 400.f, 40.f));

    int campoActivo = 0;
    CampoTexto* campoSeleccionado = nullptr;

    std::vector<std::string> listaUsuarios;
    int usuarioSeleccionado = 0;
    std::vector<sf::FloatRect> usuariosBounds;

    std::vector<EntradaCapsula> entradasCapsula;
    int entradaSeleccionada = 0;
    bool actualizandoEntradas = false;

    std::vector<bool> mostrarClave;

    // --- NUEVAS VARIABLES PARA SCROLL ---
    float scrollOffset = 0.f; // desplazamiento vertical actual en píxeles
    const float alturaFila = 40.f; // altura fija por fila (coherente con dibujo)
    const float areaAlturaMaxima = 400.f; // altura visible de la lista en píxeles
    // -----------------------------------

    const sf::Color filaPar = fondoFilaPar;
    const sf::Color filaImpar = fondoFilaImpar;
    const sf::Color filaSeleccion = filaSeleccionada;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();

            // --- Manejo del scroll con rueda del ratón ---
            if (estado == Estado::Capsula && event.type == sf::Event::MouseWheelScrolled) {
                if (event.mouseWheelScroll.wheel == sf::Mouse::VerticalWheel) {
                    float delta = event.mouseWheelScroll.delta * 30.f; // Sensibilidad, 30px por tick
                    scrollOffset -= delta; // desplazamos la lista: rueda hacia adelante baja offset

                    // Limitar scrollOffset para no sobrepasar límite inferior
                    float contenidoAltura = entradasCapsula.size() * alturaFila;
                    float maxOffset = std::max(0.f, contenidoAltura - areaAlturaMaxima);
                    if (scrollOffset < 0) scrollOffset = 0;
                    if (scrollOffset > maxOffset) scrollOffset = maxOffset;
                }
            }
            // ---------------------------------------------

            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F5) {
                try {
                    camaraDisponible = gen.hayCamaraDisponible();
                } catch (...) {
                    camaraDisponible = false;
                }
                spriteCam.setTexture(camaraDisponible ? texSiCam : texNoCam);

                if (!camaraDisponible) estado = Estado::ErrorCamara;
                else if (estado == Estado::ErrorCamara) estado = Estado::MenuPrincipal;
            }

            if (estado == Estado::MenuPrincipal) {
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2f mouse = (sf::Vector2f)sf::Mouse::getPosition(window);

                    if (mouse.x > 200 && mouse.x < 600 && mouse.y > 220 && mouse.y < 280)
                        estado = Estado::Registro;

                    if (mouse.x > 200 && mouse.x < 600 && mouse.y > 320 && mouse.y < 380) {
                        listaUsuarios = listarUsuarios();
                        usuarioSeleccionado = 0;
                        estado = Estado::LoginSeleccionUsuario;
                    }
                }
            }
            else if (estado == Estado::Registro) {
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2f mouse = (sf::Vector2f)sf::Mouse::getPosition(window);
                    campoRegistroUsuario.activo = campoRegistroUsuario.rect.contains(mouse);

                    if (mouse.x > 200 && mouse.x < 600 && mouse.y > 270 && mouse.y < 330) {
                        if (!camaraDisponible) registroMensaje = "Cámara no disponible.";
                        else if (campoRegistroUsuario.texto.empty()) registroMensaje = "Introduce un nombre de usuario.";
                        else if (fs::exists(fs::path("Capsulas") / campoRegistroUsuario.texto))
                            registroMensaje = "El usuario ya existe.";
                        else {
                            sf::Image img = gen.captureImageFromCamera();
                            if (img.getSize().x == 0) registroMensaje = "Error capturando imagen.";
                            else {
                                claveGeneradaRegistro = gen.generateKeyFromImage(img, 8);
                                estado = Estado::RegistroMostrarClave;
                            }
                        }
                    }
                    if (mouse.x > 200 && mouse.x < 600 && mouse.y > 350 && mouse.y < 410) {
                        registroMensaje.clear();
                        campoRegistroUsuario.texto.clear();
                        campoRegistroUsuario.cursorPos = 0;
                        campoRegistroUsuario.clearSeleccion();
                        estado = Estado::MenuPrincipal;
                    }
                }
                if (event.type == sf::Event::TextEntered) {
                    if (campoRegistroUsuario.activo && event.text.unicode < 128 && esAlfanumerico(static_cast<char>(event.text.unicode))) {
                        campoRegistroUsuario.insertarTexto(std::string(1, (char)event.text.unicode));
                    }
                    if (campoRegistroUsuario.activo && event.text.unicode == 8) {
                        if (campoRegistroUsuario.tieneSeleccion()) campoRegistroUsuario.borrarSeleccion();
                        else if (!campoRegistroUsuario.texto.empty() && campoRegistroUsuario.cursorPos > 0) {
                            campoRegistroUsuario.texto.erase(campoRegistroUsuario.cursorPos - 1, 1);
                            campoRegistroUsuario.cursorPos--;
                        }
                    }
                }
            }
            else if (estado == Estado::RegistroMostrarClave) {
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2f mouse = (sf::Vector2f)sf::Mouse::getPosition(window);
                    if (mouse.x > 200 && mouse.x < 600 && mouse.y > 320 && mouse.y < 380) {
                        fs::path CapsulaDir = fs::path("Capsulas") / campoRegistroUsuario.texto;
                        fs::create_directories(CapsulaDir);
                        std::string contenido = "Bienvenido a tu Capsula, " + campoRegistroUsuario.texto + "!\n";
                        std::string cifrado = cifrarXOR(contenido, claveGeneradaRegistro);
                        std::ofstream dat(CapsulaDir / (campoRegistroUsuario.texto + ".dat"), std::ios::binary);
                        dat.write(cifrado.c_str(), cifrado.size());
                        dat.close();
                        registroMensaje = "Usuario creado. Puedes iniciar sesión.";
                        campoRegistroUsuario.texto.clear();
                        campoRegistroUsuario.cursorPos = 0;
                        campoRegistroUsuario.clearSeleccion();
                        claveGeneradaRegistro.clear();
                        estado = Estado::MenuPrincipal;
                    }
                }
            }
            else if (estado == Estado::LoginSeleccionUsuario) {
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2f mouse = (sf::Vector2f)sf::Mouse::getPosition(window);
                    campoSeleccionado = nullptr;
                    for (size_t i = 0; i < usuariosBounds.size(); ++i) {
                        if (usuariosBounds[i].contains(mouse)) {
                            usuarioSeleccionado = (int)i;
                            usuarioActual = listaUsuarios[i];
                            campoPassword.texto.clear();
                            campoPassword.cursorPos = 0;
                            campoPassword.clearSeleccion();
                            estado = Estado::LoginPedirPassword;
                            campoSeleccionado = &campoPassword;
                            mensajeSistema.clear();
                            break;
                        }
                    }
                }
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Up && usuarioSeleccionado > 0) usuarioSeleccionado--;
                    if (event.key.code == sf::Keyboard::Down && usuarioSeleccionado + 1 < (int)listaUsuarios.size()) usuarioSeleccionado++;
                    if (event.key.code == sf::Keyboard::Enter && !listaUsuarios.empty()) {
                        usuarioActual = listaUsuarios[usuarioSeleccionado];
                        campoPassword.texto.clear();
                        campoPassword.cursorPos = 0;
                        campoPassword.clearSeleccion();
                        estado = Estado::LoginPedirPassword;
                        campoSeleccionado = &campoPassword;
                        mensajeSistema.clear();
                    }
                    if (event.key.code == sf::Keyboard::Escape) estado = Estado::MenuPrincipal;
                }
            }
            else if (estado == Estado::LoginPedirPassword) {
                if (event.type == sf::Event::TextEntered) {
                    if (campoPassword.activo && event.text.unicode < 128 && std::isprint(static_cast<char>(event.text.unicode))) {
                        campoPassword.insertarTexto(std::string(1, (char)event.text.unicode));
                    }
                    if (campoPassword.activo && event.text.unicode == 8) {
                        if (campoPassword.tieneSeleccion()) campoPassword.borrarSeleccion();
                        else if (!campoPassword.texto.empty() && campoPassword.cursorPos > 0) {
                            campoPassword.texto.erase(campoPassword.cursorPos - 1, 1);
                            campoPassword.cursorPos--;
                        }
                    }
                }
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.control && event.key.code == sf::Keyboard::C && campoPassword.tieneSeleccion()) {
                        sf::Clipboard::setString(campoPassword.textoSeleccionado());
                    }
                    if (event.key.control && event.key.code == sf::Keyboard::V) {
                        std::string clip = sf::Clipboard::getString();
                        if (!clip.empty()) campoPassword.insertarTexto(clip);
                    }
                    if (event.key.code == sf::Keyboard::Return) {
                        if (!camaraDisponible) mensajeSistema = "Cámara no disponible.";
                        else {
                            fs::path archivo = fs::path("Capsulas") / usuarioActual / (usuarioActual + ".dat");
                            std::ifstream dat(archivo, std::ios::binary);
                            std::ostringstream ss;
                            ss << dat.rdbuf();
                            std::string cifrado = ss.str();
                            std::string descifrado = cifrarXOR(cifrado, campoPassword.texto);
                            if (descifrado.find("Bienvenido a tu Capsula") != std::string::npos) {
                                claveActual = campoPassword.texto;
                                estado = Estado::Capsula;
                                mensajeSistema = "¡Bienvenido, " + usuarioActual + "!";
                                actualizandoEntradas = true;
                                campoPassword.activo = false;
                                campoSeleccionado = nullptr;

                                // Reiniciar scroll al entrar en cápsula para evitar confusión
                                scrollOffset = 0.f;
                            } else mensajeSistema = "Contraseña incorrecta.";
                        }
                    }
                    if (event.key.code == sf::Keyboard::Escape) {
                        estado = Estado::MenuPrincipal;
                        campoPassword.activo = false;
                        campoSeleccionado = nullptr;
                    }
                }
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2f mouse = (sf::Vector2f)sf::Mouse::getPosition(window);
                    if (campoPassword.rect.contains(mouse)) {
                        campoPassword.activo = true;
                        campoPassword.cursorPos = campoPassword.getCursorPosDesdeX(mouse.x, font, 26);
                        campoSeleccionado = &campoPassword;
                    } else {
                        campoPassword.activo = false;
                        campoSeleccionado = nullptr;
                    }
                }
            }
            else if (estado == Estado::Capsula) {
                if (actualizandoEntradas) {
                    entradasCapsula = leerEntradasCapsula(usuarioActual, claveActual);
                    mostrarClave.assign(entradasCapsula.size(), false);
                    if (entradaSeleccionada >= (int)entradasCapsula.size()) entradaSeleccionada = (int)entradasCapsula.size() - 1;
                    if (entradaSeleccionada < 0) entradaSeleccionada = 0;
                    actualizandoEntradas = false;

                    // Reiniciar scroll al actualizar lista para evitar errores
                    scrollOffset = 0.f;
                }
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Escape) {
                        usuarioActual.clear();
                        claveActual.clear();
                        campoPassword.texto.clear();
                        campoPassword.cursorPos = 0;
                        campoPassword.clearSeleccion();
                        estado = Estado::MenuPrincipal;
                    }
                    if (event.key.code == sf::Keyboard::Down && entradaSeleccionada + 1 < (int)entradasCapsula.size())
                        entradaSeleccionada++;
                    if (event.key.code == sf::Keyboard::Up && entradaSeleccionada > 0)
                        entradaSeleccionada--;
                    if (event.key.code == sf::Keyboard::A) {
                        campoNuevoServicio.texto.clear();
                        campoNuevoServicio.cursorPos = 0;
                        campoNuevoServicio.clearSeleccion();
                        campoNuevoUsuario.texto.clear();
                        campoNuevoUsuario.cursorPos = 0;
                        campoNuevoUsuario.clearSeleccion();
                        campoNuevaClave.texto.clear();
                        campoNuevaClave.cursorPos = 0;
                        campoNuevaClave.clearSeleccion();
                        campoActivo = 0;
                        estado = Estado::CapsulaAñadir;
                    }
                    if (event.key.code == sf::Keyboard::Delete && !entradasCapsula.empty()) {
                        entradasCapsula.erase(entradasCapsula.begin() + entradaSeleccionada);
                        mostrarClave.erase(mostrarClave.begin() + entradaSeleccionada);
                        guardarEntradasCapsula(usuarioActual, claveActual, entradasCapsula);
                        if (entradaSeleccionada >= (int)entradasCapsula.size())
                            entradaSeleccionada = (int)entradasCapsula.size() - 1;
                        actualizandoEntradas = true;
                    }
                }
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2f mouse = (sf::Vector2f)sf::Mouse::getPosition(window);
                    float yInicio = 230;
                    const float anchoTabla = 400.f;
                    const float centroX = (800 - anchoTabla) / 2.f;

                    sf::FloatRect areaAñadir(centroX, 150, anchoTabla, 40.f);
                    if (areaAñadir.contains(mouse)) {
                        campoNuevoServicio.texto.clear();
                        campoNuevoServicio.cursorPos = 0;
                        campoNuevoServicio.clearSeleccion();
                        campoNuevoUsuario.texto.clear();
                        campoNuevoUsuario.cursorPos = 0;
                        campoNuevoUsuario.clearSeleccion();
                        campoNuevaClave.texto.clear();
                        campoNuevaClave.cursorPos = 0;
                        campoNuevaClave.clearSeleccion();
                        campoActivo = 0;
                        estado = Estado::CapsulaAñadir;
                    }

                    // Ahora para los botones en filas, consideramos scrollOffset:
                    for (size_t i = 0; i < entradasCapsula.size(); ++i) {
                        float yFila = yInicio + alturaFila * i - scrollOffset;

                        // Solo considerar filas visibles para optimizar y evitar clics invisibles
                        if (yFila + alturaFila < 150 || yFila > 600) continue;

                        sf::FloatRect botonMostrar(centroX + 270 + 160, yFila + 7, 30.f, 30.f);
                        if (botonMostrar.contains(mouse)) {
                            mostrarClave[i] = !mostrarClave[i];
                            break;
                        }

                        sf::FloatRect botonBorrar(centroX + 270 + 200, yFila + 7, 30.f, 30.f);
                        if (botonBorrar.contains(mouse)) {
                            entradasCapsula.erase(entradasCapsula.begin() + i);
                            mostrarClave.erase(mostrarClave.begin() + i);
                            guardarEntradasCapsula(usuarioActual, claveActual, entradasCapsula);
                            if (entradaSeleccionada >= (int)entradasCapsula.size())
                                entradaSeleccionada = (int)entradasCapsula.size() - 1;
                            actualizandoEntradas = true;
                            break;
                        }
                    }

                    // Botón Exportar CSV: ampliado para toda el área del botón
                    sf::FloatRect botonExportarRect(centroX, 580, 180, 40);
                    if (botonExportarRect.contains(mouse)) {
                        std::string carpeta = seleccionarCarpetaZenity();
                        if (!carpeta.empty()) {
                            fs::path rutaCSV = fs::path(carpeta) / (usuarioActual + "_export.csv");
                            try {
                                std::ofstream csv(rutaCSV);
                                if (!csv) throw std::runtime_error("No se pudo abrir archivo para escritura");
                                csv << "Servicio,Usuario,Clave\n";
                                for (const auto& e : entradasCapsula) {
                                    csv << escapeCSV(e.servicio) << "," << escapeCSV(e.usuario) << "," << escapeCSV(e.clave) << "\n";
                                }
                                csv.close();
                                std::cout << "Exportación CSV exitosa: " << rutaCSV << std::endl;
                            } catch (...) {
                                std::cout << "Error al exportar CSV.\n";
                            }
                        }
                    }
                    // Botón Importar CSV: ampliado para toda el área del botón
                    sf::FloatRect botonImportarRect(centroX + 220, 580, 180, 40);
                    if (botonImportarRect.contains(mouse)) {
                        std::string rutaCSV = seleccionarArchivoZenity();
                        if (!rutaCSV.empty()) {
                            if (importarCapsulaDesdeCSV(rutaCSV, entradasCapsula)) {
                                guardarEntradasCapsula(usuarioActual, claveActual, entradasCapsula);
                                actualizandoEntradas = true;
                                std::cout << "Importación CSV completada.\n";
                            } else {
                                std::cout << "Error al importar CSV.\n";
                            }
                        }
                    }
                }
            }
            else if (estado == Estado::CapsulaAñadir) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Escape) {
                        estado = Estado::Capsula;
                        actualizandoEntradas = true;
                    }
                    if (event.key.code == sf::Keyboard::Return) {
                        if (!campoNuevoServicio.texto.empty() && !campoNuevoUsuario.texto.empty() && !campoNuevaClave.texto.empty()) {
                            entradasCapsula.push_back({ campoNuevoServicio.texto, campoNuevoUsuario.texto, campoNuevaClave.texto });
                            mostrarClave.push_back(false);
                            guardarEntradasCapsula(usuarioActual, claveActual, entradasCapsula);
                            estado = Estado::Capsula;
                            actualizandoEntradas = true;
                        }
                    }
                    if (event.key.code == sf::Keyboard::Tab) {
                        campoActivo = (campoActivo + 1) % 3;
                    }
                    if (event.key.code == sf::Keyboard::Up) {
                        campoActivo = (campoActivo + 2) % 3;
                    }
                    if (event.key.code == sf::Keyboard::Down) {
                        campoActivo = (campoActivo + 1) % 3;
                    }
                }
                if (event.type == sf::Event::TextEntered) {
                    if (event.text.unicode < 128 && std::isprint(static_cast<char>(event.text.unicode))) {
                        char c = static_cast<char>(event.text.unicode);
                        if (campoActivo == 0) campoNuevoServicio.insertarTexto(std::string(1, c));
                        else if (campoActivo == 1) campoNuevoUsuario.insertarTexto(std::string(1, c));
                        else if (campoActivo == 2) campoNuevaClave.insertarTexto(std::string(1, c));
                    }
                    if (event.text.unicode == 8) {
                        if (campoActivo == 0 && campoNuevoServicio.tieneSeleccion()) campoNuevoServicio.borrarSeleccion();
                        else if (campoActivo == 0 && !campoNuevoServicio.texto.empty() && campoNuevoServicio.cursorPos > 0) {
                            campoNuevoServicio.texto.erase(campoNuevoServicio.cursorPos - 1, 1);
                            campoNuevoServicio.cursorPos--;
                        }
                        else if (campoActivo == 1 && campoNuevoUsuario.tieneSeleccion()) campoNuevoUsuario.borrarSeleccion();
                        else if (campoActivo == 1 && !campoNuevoUsuario.texto.empty() && campoNuevoUsuario.cursorPos > 0) {
                            campoNuevoUsuario.texto.erase(campoNuevoUsuario.cursorPos - 1, 1);
                            campoNuevoUsuario.cursorPos--;
                        }
                        else if (campoActivo == 2 && campoNuevaClave.tieneSeleccion()) campoNuevaClave.borrarSeleccion();
                        else if (campoActivo == 2 && !campoNuevaClave.texto.empty() && campoNuevaClave.cursorPos > 0) {
                            campoNuevaClave.texto.erase(campoNuevaClave.cursorPos - 1, 1);
                            campoNuevaClave.cursorPos--;
                        }
                    }
                }
                if (event.type == sf::Event::MouseButtonPressed) {
                    sf::Vector2f mouse = (sf::Vector2f)sf::Mouse::getPosition(window);
                    campoNuevoServicio.activo = campoNuevoServicio.rect.contains(mouse);
                    campoNuevoUsuario.activo = campoNuevoUsuario.rect.contains(mouse);
                    campoNuevaClave.activo = campoNuevaClave.rect.contains(mouse);
                    if (campoNuevoServicio.activo) campoActivo = 0;
                    else if (campoNuevoUsuario.activo) campoActivo = 1;
                    else if (campoNuevaClave.activo) campoActivo = 2;
                }
            }

            if (campoSeleccionado) campoSeleccionado->updateCursorBlink();
            if (estado == Estado::Registro) campoRegistroUsuario.updateCursorBlink();
            if (estado == Estado::LoginPedirPassword) campoPassword.updateCursorBlink();
            if (estado == Estado::CapsulaAñadir) {
                campoNuevoServicio.updateCursorBlink();
                campoNuevoUsuario.updateCursorBlink();
                campoNuevaClave.updateCursorBlink();
            }
        }

        window.clear(fondoPrincipal);

        if (estado == Estado::MenuPrincipal) {
            sf::CircleShape puntoVerde(6.f);
            puntoVerde.setFillColor(sf::Color(0, 255, 0));
            puntoVerde.setPosition(780.f, 10.f);
            window.draw(puntoVerde);

            float desiredWidth = 150.f;
            sf::Vector2u texSize = texTree.getSize();
            float scaleFactor = desiredWidth / texSize.x;
            spriteTree.setScale(scaleFactor, scaleFactor);
            float posX = 200.f + 400.f / 2.f - desiredWidth / 2.f;
            float posY = 320.f - texSize.y * scaleFactor - 130.f;
            spriteTree.setPosition(posX, posY);
            window.draw(spriteTree);
        }

        sf::Text titulo;
        titulo.setFont(font);
        titulo.setCharacterSize(32);
        {
            std::string str = "Raiz - Gestor de Contraseñas";
            titulo.setString(sf::String::fromUtf8(str.begin(), str.end()));
        }
        titulo.setFillColor(azulLinaje);
        titulo.setPosition((800 - titulo.getLocalBounds().width) / 2, 20);
        window.draw(titulo);

        if (!camaraDisponible) {
            sf::Text error;
            error.setFont(font);
            error.setCharacterSize(20);
            {
                std::string str = "ERROR: Se necesita cámara.\nPulsa F5 para reintentar.";
                error.setString(sf::String::fromUtf8(str.begin(), str.end()));
            }
            error.setFillColor(errorColor);
            error.setPosition(480, 520);
            window.draw(error);
        }

        const float anchoBoton = 400.f;
        const float altoBoton = 60.f;
        const float centroX = (800 - anchoBoton) / 2;

        if (estado == Estado::MenuPrincipal) {
            sf::RectangleShape botonCrear(sf::Vector2f(anchoBoton, altoBoton));
            botonCrear.setFillColor(azulLinaje);
            botonCrear.setPosition(centroX, 220);
            window.draw(botonCrear);

            sf::Text txtCrear;
            txtCrear.setFont(font);
            txtCrear.setCharacterSize(28);
            {
                std::string s = "Crear usuario";
                txtCrear.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            txtCrear.setFillColor(textoClaro);
            txtCrear.setPosition(centroX + (anchoBoton - txtCrear.getLocalBounds().width) / 2, 230);
            window.draw(txtCrear);

            sf::RectangleShape botonLogin(sf::Vector2f(anchoBoton, altoBoton));
            botonLogin.setFillColor(azulClaro);
            botonLogin.setPosition(centroX, 320);
            window.draw(botonLogin);

            sf::Text txtLogin;
            txtLogin.setFont(font);
            txtLogin.setCharacterSize(28);
            {
                std::string s = "Iniciar sesión";
                txtLogin.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            txtLogin.setFillColor(textoClaro);
            txtLogin.setPosition(centroX + (anchoBoton - txtLogin.getLocalBounds().width) / 2, 330);
            window.draw(txtLogin);

            if (!registroMensaje.empty()) {
                sf::Text msj;
                msj.setFont(font);
                msj.setCharacterSize(20);
                msj.setFillColor(errorColor);
                msj.setPosition(centroX, 430);
                msj.setString(sf::String::fromUtf8(registroMensaje.begin(), registroMensaje.end()));
                window.draw(msj);
            }
        }
        else if (estado == Estado::Registro) {
            sf::Text etiqueta;
            etiqueta.setFont(font);
            etiqueta.setCharacterSize(24);
            {
                std::string s = "Introduce un nombre de usuario:";
                etiqueta.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            etiqueta.setFillColor(azulClaro);
            etiqueta.setPosition(centroX, 150);
            window.draw(etiqueta);

            campoRegistroUsuario.dibujar(window, font, 26, textoClaro, azulClaro);

            sf::RectangleShape botonCrear(sf::Vector2f(anchoBoton, altoBoton));
            botonCrear.setFillColor(azulLinaje);
            botonCrear.setPosition(centroX, 270);
            window.draw(botonCrear);

            sf::Text txtCrear;
            txtCrear.setFont(font);
            txtCrear.setCharacterSize(26);
            {
                std::string s = "Crear usuario";
                txtCrear.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            txtCrear.setFillColor(textoClaro);
            txtCrear.setPosition(centroX + (anchoBoton - txtCrear.getLocalBounds().width) / 2, 280);
            window.draw(txtCrear);

            sf::RectangleShape botonVolver(sf::Vector2f(anchoBoton, altoBoton));
            botonVolver.setFillColor(azulClaro);
            botonVolver.setPosition(centroX, 350);
            window.draw(botonVolver);

            sf::Text txtVolver;
            txtVolver.setFont(font);
            txtVolver.setCharacterSize(26);
            {
                std::string s = "Volver";
                txtVolver.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            txtVolver.setFillColor(textoClaro);
            txtVolver.setPosition(centroX + (anchoBoton - txtVolver.getLocalBounds().width) / 2, 360);
            window.draw(txtVolver);

            if (!registroMensaje.empty()) {
                sf::Text msj;
                msj.setFont(font);
                msj.setCharacterSize(20);
                msj.setFillColor(errorColor);
                msj.setPosition(centroX, 430);
                msj.setString(sf::String::fromUtf8(registroMensaje.begin(), registroMensaje.end()));
                window.draw(msj);
            }
        }
        else if (estado == Estado::RegistroMostrarClave) {
            sf::Text titulo;
            titulo.setFont(font);
            titulo.setCharacterSize(28);
            {
                std::string s = "¡Guarda tu contraseña!";
                titulo.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            titulo.setFillColor(azulLinaje);
            titulo.setPosition(centroX, 150);
            window.draw(titulo);

            std::string claveTxt = "Clave generada: " + claveGeneradaRegistro;
            sf::Text clave;
            clave.setFont(font);
            clave.setCharacterSize(24);
            clave.setFillColor(textoClaro);
            clave.setPosition(centroX, 220);
            clave.setString(sf::String::fromUtf8(claveTxt.begin(), claveTxt.end()));
            window.draw(clave);

            sf::RectangleShape botonConfirmar(sf::Vector2f(anchoBoton, altoBoton));
            botonConfirmar.setFillColor(azulLinaje);
            botonConfirmar.setPosition(centroX, 320);
            window.draw(botonConfirmar);

            sf::Text txtConfirmar;
            txtConfirmar.setFont(font);
            txtConfirmar.setCharacterSize(26);
            {
                std::string s = "Confirmar";
                txtConfirmar.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            txtConfirmar.setFillColor(textoClaro);
            txtConfirmar.setPosition(centroX + (anchoBoton - txtConfirmar.getLocalBounds().width) / 2, 330);
            window.draw(txtConfirmar);
        }
        else if (estado == Estado::LoginSeleccionUsuario) {
            sf::Text titulo;
            titulo.setFont(font);
            titulo.setCharacterSize(28);
            {
                std::string s = "Selecciona tu usuario";
                titulo.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            titulo.setFillColor(azulLinaje);
            titulo.setPosition(centroX, 100);
            window.draw(titulo);

            usuariosBounds.clear();
            float yInicio = 160;
            const float alturaFila = 35;
            for (size_t i = 0; i < listaUsuarios.size(); ++i) {
                sf::RectangleShape fondo(sf::Vector2f(anchoBoton, alturaFila));
                fondo.setPosition(centroX, yInicio + alturaFila * i);
                if ((int)i == usuarioSeleccionado)
                    fondo.setFillColor(filaSeleccion);
                else if (i % 2 == 0)
                    fondo.setFillColor(filaPar);
                else
                    fondo.setFillColor(filaImpar);
                window.draw(fondo);

                sf::Text usuarioTexto;
                usuarioTexto.setFont(font);
                usuarioTexto.setCharacterSize(24);
                usuarioTexto.setFillColor(textoClaro);
                usuarioTexto.setPosition(centroX + 10, yInicio + alturaFila * i + 5);
                usuarioTexto.setString(sf::String::fromUtf8(listaUsuarios[i].begin(), listaUsuarios[i].end()));
                window.draw(usuarioTexto);

                usuariosBounds.push_back(fondo.getGlobalBounds());
            }

            sf::Text info;
            info.setFont(font);
            info.setCharacterSize(20);
            {
                std::string s = "Flechas para navegar, ENTER para seleccionar, ESC para volver.";
                info.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            info.setFillColor(textoClaro);
            info.setPosition(centroX, 576);
            window.draw(info);
        }
        else if (estado == Estado::LoginPedirPassword) {
            sf::Text titulo;
            titulo.setFont(font);
            titulo.setCharacterSize(28);
            {
                std::string s = "Contraseña para usuario: " + usuarioActual;
                titulo.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            titulo.setFillColor(azulLinaje);
            titulo.setPosition(centroX, 150);
            window.draw(titulo);

            campoPassword.dibujar(window, font, 26, textoClaro, azulClaro);

            if (!mensajeSistema.empty()) {
                sf::Text msj;
                msj.setFont(font);
                msj.setCharacterSize(20);
                msj.setFillColor(errorColor);
                msj.setPosition(centroX, 220);
                msj.setString(sf::String::fromUtf8(mensajeSistema.begin(), mensajeSistema.end()));
                window.draw(msj);
            }
        }
        else if (estado == Estado::Capsula) {
            sf::Text titulo;
            titulo.setFont(font);
            titulo.setCharacterSize(28);
            {
                std::string s = "Capsula de " + usuarioActual;
                titulo.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            titulo.setFillColor(azulLinaje);
            titulo.setPosition(centroX, 100);
            window.draw(titulo);

            sf::RectangleShape botonAñadir(sf::Vector2f(anchoBoton, 40));
            botonAñadir.setFillColor(azulLinaje);
            botonAñadir.setPosition(centroX, 150);
            window.draw(botonAñadir);

            sf::Text txtAñadir;
            txtAñadir.setFont(font);
            txtAñadir.setCharacterSize(22);
            {
                std::string s = "Añadir entrada";
                txtAñadir.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            txtAñadir.setFillColor(textoClaro);
            txtAñadir.setPosition(centroX + (anchoBoton - txtAñadir.getLocalBounds().width) / 2, 155);
            window.draw(txtAñadir);

            sf::Text cabServicio, cabUsuario, cabClave;
            cabServicio.setFont(font);
            cabUsuario.setFont(font);
            cabClave.setFont(font);
            cabServicio.setCharacterSize(22);
            cabUsuario.setCharacterSize(22);
            cabClave.setCharacterSize(22);
            {
                std::string s = "Servicio";
                cabServicio.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            {
                std::string s = "Usuario";
                cabUsuario.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            {
                std::string s = "Clave";
                cabClave.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            cabServicio.setFillColor(azulClaro);
            cabUsuario.setFillColor(azulClaro);
            cabClave.setFillColor(azulClaro);
            float xServicio = centroX + -150;
            float xUsuario = centroX + 150;
            float xClave = centroX + 270;
            cabServicio.setPosition(xServicio, 190);
            cabUsuario.setPosition(xUsuario, 190);
            cabClave.setPosition(xClave, 190);
            window.draw(cabServicio);
            window.draw(cabUsuario);
            window.draw(cabClave);

            // DIBUJO DE LISTA DESPLAZABLE CON SCROLL
            float yInicio = 230;
            for (size_t i = 0; i < entradasCapsula.size(); ++i) {
                float yFila = yInicio + alturaFila * i - scrollOffset;

                // Solo dibujar filas visibles para optimizar
                if (yFila + alturaFila < 150 || yFila > 600) continue;

                sf::Color fondo = (i == (size_t)entradaSeleccionada) ? filaSeleccion : ((i % 2 == 0) ? filaPar : filaImpar);
                sf::RectangleShape fila(sf::Vector2f(anchoBoton, alturaFila));
                fila.setPosition(centroX, yFila);
                fila.setFillColor(fondo);
                window.draw(fila);

                sf::Text txtServicio, txtUsuario, txtClave, btnMostrar, btnBorrar;
                txtServicio.setFont(font);
                txtUsuario.setFont(font);
                txtClave.setFont(font);
                btnMostrar.setFont(font);
                btnBorrar.setFont(font);
                txtServicio.setCharacterSize(20);
                txtUsuario.setCharacterSize(20);
                txtClave.setCharacterSize(20);
                btnMostrar.setCharacterSize(20);
                btnBorrar.setCharacterSize(20);
                txtServicio.setFillColor(textoClaro);
                txtUsuario.setFillColor(textoClaro);
                txtClave.setFillColor(textoClaro);
                btnMostrar.setFillColor(azulLinaje);
                btnBorrar.setFillColor(errorColor);
                txtServicio.setString(sf::String::fromUtf8(entradasCapsula[i].servicio.begin(), entradasCapsula[i].servicio.end()));
                txtUsuario.setString(sf::String::fromUtf8(entradasCapsula[i].usuario.begin(), entradasCapsula[i].usuario.end()));
                std::string claveMostrar = mostrarClave[i] ? entradasCapsula[i].clave : std::string(entradasCapsula[i].clave.size(), '*');
                txtClave.setString(sf::String::fromUtf8(claveMostrar.begin(), claveMostrar.end()));
                {
                    std::string s = mostrarClave[i] ? "🙈" : "👁️";
                    btnMostrar.setString(sf::String::fromUtf8(s.begin(), s.end()));
                }
                {
                    std::string s = "X";
                    btnBorrar.setString(sf::String::fromUtf8(s.begin(), s.end()));
                }
                txtServicio.setPosition(xServicio, yFila + 7);
                txtUsuario.setPosition(xUsuario, yFila + 7);
                txtClave.setPosition(xClave, yFila + 7);
                btnMostrar.setPosition(xClave + 160, yFila + 7);
                btnBorrar.setPosition(xClave + 200, yFila + 7);
                window.draw(txtServicio);
                window.draw(txtUsuario);
                window.draw(txtClave);
                window.draw(btnMostrar);
                window.draw(btnBorrar);
            }

            // Botones Exportar e Importar CSV visibles y activos (dibujados ya arriba)
            sf::RectangleShape botonExportar(sf::Vector2f(180, 40));
            botonExportar.setFillColor(azulLinaje);
            botonExportar.setPosition(centroX, 580);
            window.draw(botonExportar);

            sf::Text txtExportar;
            txtExportar.setFont(font);
            txtExportar.setCharacterSize(18);
            {
                std::string s = "Exportar CSV";
                txtExportar.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            txtExportar.setFillColor(textoClaro);
            txtExportar.setPosition(centroX + (180 - txtExportar.getLocalBounds().width) / 2, 585);
            window.draw(txtExportar);

            sf::RectangleShape botonImportar(sf::Vector2f(180, 40));
            botonImportar.setFillColor(azulLinaje);
            botonImportar.setPosition(centroX + 220, 580);
            window.draw(botonImportar);

            sf::Text txtImportar;
            txtImportar.setFont(font);
            txtImportar.setCharacterSize(18);
            {
                std::string s = "Importar CSV";
                txtImportar.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            txtImportar.setFillColor(textoClaro);
            txtImportar.setPosition(centroX + 220 + (180 - txtImportar.getLocalBounds().width) / 2, 585);
            window.draw(txtImportar);

            sf::Text info;
            info.setFont(font);
            info.setCharacterSize(20);
            {
                std::string s = "Flechas para navegar | A para añadir | Supr para borrar | ESC para salir";
                info.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            info.setFillColor(textoClaro);
            info.setPosition(centroX, 630);
            window.draw(info);
        }
        else if (estado == Estado::CapsulaAñadir) {
            sf::Text titulo;
            titulo.setFont(font);
            titulo.setCharacterSize(28);
            {
                std::string s = "Añadir entrada";
                titulo.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            titulo.setFillColor(azulLinaje);
            titulo.setPosition(centroX, 100);
            window.draw(titulo);

            sf::Text etiquetaServicio, etiquetaUsuario, etiquetaClave;
            etiquetaServicio.setFont(font);
            etiquetaUsuario.setFont(font);
            etiquetaClave.setFont(font);
            etiquetaServicio.setCharacterSize(20);
            etiquetaUsuario.setCharacterSize(20);
            etiquetaClave.setCharacterSize(20);
            {
                std::string s = "Servicio:";
                etiquetaServicio.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            {
                std::string s = "Usuario:";
                etiquetaUsuario.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            {
                std::string s = "Clave:";
                etiquetaClave.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            etiquetaServicio.setFillColor(azulClaro);
            etiquetaUsuario.setFillColor(azulClaro);
            etiquetaClave.setFillColor(azulClaro);
            etiquetaServicio.setPosition(centroX, 180);
            etiquetaUsuario.setPosition(centroX, 250);
            etiquetaClave.setPosition(centroX, 320);
            window.draw(etiquetaServicio);
            window.draw(etiquetaUsuario);
            window.draw(etiquetaClave);

            campoNuevoServicio.dibujar(window, font, 20, textoClaro, azulClaro);
            campoNuevoUsuario.dibujar(window, font, 20, textoClaro, azulClaro);
            campoNuevaClave.dibujar(window, font, 20, textoClaro, azulClaro);

            sf::Text instrucciones;
            instrucciones.setFont(font);
            instrucciones.setCharacterSize(18);
            {
                std::string s = "ENTER=guardar | TAB/Flechas=cambiar campo | ESC=cancelar";
                instrucciones.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            instrucciones.setFillColor(textoClaro);
            instrucciones.setPosition(centroX, 400);
            window.draw(instrucciones);
        }
        else if (estado == Estado::ErrorCamara) {
            sf::Text error;
            error.setFont(font);
            error.setCharacterSize(28);
            {
                std::string s = "Error: No se detecta cámara";
                error.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            error.setFillColor(errorColor);
            error.setPosition(centroX, 300);
            window.draw(error);

            sf::Text instruccion;
            instruccion.setFont(font);
            instruccion.setCharacterSize(20);
            {
                std::string s = "Pulsa F5 para reintentar";
                instruccion.setString(sf::String::fromUtf8(s.begin(), s.end()));
            }
            instruccion.setFillColor(textoClaro);
            instruccion.setPosition(centroX, 350);
            window.draw(instruccion);
        }

        window.display();
    }
    return 0;
}
