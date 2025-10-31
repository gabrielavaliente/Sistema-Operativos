#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <random>
#include <atomic>
using namespace std;


int recurso;
int lectores_activos;
int escritores_esperando;
bool escribiendo;
mutex mtx, mtx_escritura;
condition_variable puede_leer, puede_escribir;

atomic<int> total_lecturas;
atomic<int> total_escrituras;
atomic<long long> tiempo_espera_lectores;
atomic<long long> tiempo_espera_escritores;


void reiniciarVariables() {
    recurso = 0;
    lectores_activos = 0;
    escritores_esperando = 0;
    escribiendo = false;
    total_lecturas = 0;
    total_escrituras = 0;
    tiempo_espera_lectores = 0;
    tiempo_espera_escritores = 0;
}

void prioridadLectores() {
    reiniciarVariables();
    cout << "\n=== Simulación: PRIORIDAD A LECTORES ===\n";

    auto lector = [&](int id) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dist(10, 50);

        for (int i = 0; i < 50; ++i) {
            auto inicio_espera = chrono::high_resolution_clock::now();

            {
                lock_guard<mutex> lock(mtx);
                lectores_activos++;
                if (lectores_activos == 1)
                    mtx_escritura.lock(); // primer lector bloquea escritores
            }

            auto fin_espera = chrono::high_resolution_clock::now();
            tiempo_espera_lectores += chrono::duration_cast<chrono::microseconds>(fin_espera - inicio_espera).count();

            cout << "👀 Lector " << id << " está leyendo el valor: " << recurso << endl;
            this_thread::sleep_for(chrono::milliseconds(dist(gen)));

            {
                lock_guard<mutex> lock(mtx);
                lectores_activos--;
                if (lectores_activos == 0)
                    mtx_escritura.unlock(); // último lector libera escritores
            }

            total_lecturas++;
            this_thread::sleep_for(chrono::milliseconds(dist(gen)));
        }
    };

    auto escritor = [&](int id) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dist(20, 80);

        for (int i = 0; i < 50; ++i) {
            auto inicio_espera = chrono::high_resolution_clock::now();
            mtx_escritura.lock(); // exclusión mutua para escritura
            auto fin_espera = chrono::high_resolution_clock::now();

            tiempo_espera_escritores += chrono::duration_cast<chrono::microseconds>(fin_espera - inicio_espera).count();

            recurso++;
            cout << "📝 Escritor " << id << " modificó el recurso a: " << recurso << endl;
            this_thread::sleep_for(chrono::milliseconds(dist(gen)));

            mtx_escritura.unlock();
            total_escrituras++;
            this_thread::sleep_for(chrono::milliseconds(dist(gen)));
        }
    };

    vector<thread> lectores, escritores;
    for (int i = 1; i <= 8; ++i) lectores.emplace_back(lector, i);
    for (int i = 1; i <= 3; ++i) escritores.emplace_back(escritor, i);

    for (auto &t : lectores) t.join();
    for (auto &t : escritores) t.join();

    cout << "\n--- Resultados PRIORIDAD A LECTORES ---" << endl;
    cout << "Lecturas totales: " << total_lecturas.load() << endl;
    cout << "Escrituras totales: " << total_escrituras.load() << endl;
    cout << "Promedio espera lectores: " << (double)tiempo_espera_lectores / max(1, total_lecturas.load()) << " μs\n";
    cout << "Promedio espera escritores: " << (double)tiempo_espera_escritores / max(1, total_escrituras.load()) << " μs\n";
    cout << "---------------------------------------\n";
}

void prioridadEscritores() {
    reiniciarVariables();
    cout << "\n=== Simulación: PRIORIDAD A ESCRITORES ===\n";

    auto lector = [&](int id) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dist(10, 50);

        for (int i = 0; i < 50; ++i) {
            auto inicio_espera = chrono::high_resolution_clock::now();

            unique_lock<mutex> lock(mtx);
            while (escritores_esperando > 0 || escribiendo)
                puede_leer.wait(lock);

            auto fin_espera = chrono::high_resolution_clock::now();
            tiempo_espera_lectores += chrono::duration_cast<chrono::microseconds>(fin_espera - inicio_espera).count();

            lectores_activos++;
            lock.unlock();

            cout << "👀 Lector " << id << " está leyendo el valor: " << recurso << endl;
            this_thread::sleep_for(chrono::milliseconds(dist(gen)));

            lock.lock();
            lectores_activos--;
            if (lectores_activos == 0)
                puede_escribir.notify_one();
            lock.unlock();

            total_lecturas++;
            this_thread::sleep_for(chrono::milliseconds(dist(gen)));
        }
    };

    auto escritor = [&](int id) {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dist(20, 80);

        for (int i = 0; i < 50; ++i) {
            auto inicio_espera = chrono::high_resolution_clock::now();

            unique_lock<mutex> lock(mtx);
            escritores_esperando++;
            while (lectores_activos > 0 || escribiendo)
                puede_escribir.wait(lock);
            escritores_esperando--;
            escribiendo = true;
            lock.unlock();

            auto fin_espera = chrono::high_resolution_clock::now();
            tiempo_espera_escritores += chrono::duration_cast<chrono::microseconds>(fin_espera - inicio_espera).count();

            recurso++;
            cout << "📝 Escritor " << id << " modificó el recurso a: " << recurso << endl;
            this_thread::sleep_for(chrono::milliseconds(dist(gen)));

            lock.lock();
            escribiendo = false;
            if (escritores_esperando > 0)
                puede_escribir.notify_one();
            else
                puede_leer.notify_all();
            lock.unlock();

            total_escrituras++;
            this_thread::sleep_for(chrono::milliseconds(dist(gen)));
        }
    };

    vector<thread> lectores, escritores;
    for (int i = 1; i <= 8; ++i) lectores.emplace_back(lector, i);
    for (int i = 1; i <= 3; ++i) escritores.emplace_back(escritor, i);

    for (auto &t : lectores) t.join();
    for (auto &t : escritores) t.join();

    cout << "\n--- Resultados PRIORIDAD A ESCRITORES ---" << endl;
    cout << "Lecturas totales: " << total_lecturas.load() << endl;
    cout << "Escrituras totales: " << total_escrituras.load() << endl;
    cout << "Promedio espera lectores: " << (double)tiempo_espera_lectores / max(1, total_lecturas.load()) << " μs\n";
    cout << "Promedio espera escritores: " << (double)tiempo_espera_escritores / max(1, total_escrituras.load()) << " μs\n";
    cout << "-----------------------------------------\n";
}

int main() {
    int opcion;
    do {
        cout << "\n============================================" << endl;
        cout << "    SIMULADOR LECTORES–ESCRITORES (SO)     " << endl;
        cout << "============================================" << endl;
        cout << "1. Ejecutar Prioridad a Lectores" << endl;
        cout << "2. Ejecutar Prioridad a Escritores" << endl;
        cout << "3. Salir del Programa" << endl;
        cout << "Seleccione una opción: ";
        cin >> opcion;

        switch (opcion) {
            case 1: prioridadLectores(); break;
            case 2: prioridadEscritores(); break;
            case 3: cout << "\nSaliendo del programa. \n"; break;
            default: cout << "\nOpción inválida. Intente de nuevo.\n"; break;
        }

    } while (opcion != 3);

    return 0;
}