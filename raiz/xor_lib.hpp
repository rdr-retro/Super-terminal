#ifndef XOR_LIB_HPP
#define XOR_LIB_HPP

#include <SFML/Graphics.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <random>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <stdexcept>

namespace xor_lib {

class Generator {
public:
    // 1. Constructor: inicializa variables y abre cámara si es posible
    Generator();

    // 2. Destructor: detiene hilo y libera cámara
    ~Generator();

    // 3. Comprueba si la cámara está disponible
    bool hayCamaraDisponible();

    // 4. Captura imagen desde la cámara
    sf::Image captureImageFromCamera();

    // 5. Convierte cv::Mat de OpenCV a sf::Image de SFML
    sf::Image cvMatToSfImage(const cv::Mat& mat);

    // 6. Genera clave desde imagen y longitud
    std::string generateKeyFromImage(const sf::Image& image, size_t length = 1024);

    // 7. Genera una clave única (modo pausado)
    std::string generateSingleKey(const sf::Image& image, size_t length = 1024);

    // 8. Inicia generación indefinida de claves (modo background)
    void startIndefiniteGeneration(std::function<void(const std::string&)> callback, size_t length = 1024, unsigned int intervalMs = 1000);

    // 9. Pausa la generación indefinida
    void pauseGeneration();

    // 10. Reanuda la generación indefinida
    void resumeGeneration();

    // 11. Detiene la generación indefinida y espera a que termine
    void stopGeneration();

    // 12. Cifra/descifra texto con XOR usando clave
    std::string cifrarXOR(const std::string& texto, const std::string& clave);

    // 13. Estado si está generando indefinidamente
    bool isGenerating() const;

    // 14. Estado si está pausado
    bool isPaused() const;

    // 15. Cambia longitud clave para generación indefinida
    void setKeyLength(size_t length);

    // 16. Cambia intervalo de generación indefinida (ms)
    void setGenerationInterval(unsigned int intervalMs);

private:
    // Hilo para generación indefinida
    void generationThreadFunc();

    std::thread generationThread;
    std::atomic<bool> running;
    std::atomic<bool> paused;
    std::mutex mtx;
    std::condition_variable cv;
    std::function<void(const std::string&)> callbackFunc;
    size_t keyLength;
    unsigned int generationIntervalMs;
    std::mutex cameraMutex;
    cv::VideoCapture cap;
};

} // namespace xor_lib

#endif // XOR_LIB_HPP
