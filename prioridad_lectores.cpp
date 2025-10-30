#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <random>
using namespace std;

// Variables compartidas
int recurso = 0;
int lectores_activos = 0;

// Mutex
mutex mtx; // protege lectores_activos
mutex mtx_escritura; // protege el acceso exclusivo al recurso

void lector(int id) {
random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> dist(100, 300);

while (true) {
{
lock_guard<mutex> lock(mtx);
lectores_activos++;
if (lectores_activos == 1)
mtx_escritura.lock(); // el primer lector bloquea escritores
}

// --- Sección crítica (lectura) ---
cout << "👀 Lector " << id << " está leyendo el valor: " << recurso << endl;
this_thread::sleep_for(chrono::milliseconds(dist(gen)));

{
lock_guard<mutex> lock(mtx);
lectores_activos--;
if (lectores_activos == 0)
mtx_escritura.unlock(); // el último lector libera escritores
}

this_thread::sleep_for(chrono::milliseconds(dist(gen)));
}
}

void escritor(int id) {
random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> dist(200, 500);

while (true) {
mtx_escritura.lock();
recurso++;
cout << "📝 Escritor " << id << " modificó el recurso a: " << recurso << endl;
this_thread::sleep_for(chrono::milliseconds(dist(gen)));
mtx_escritura.unlock();
this_thread::sleep_for(chrono::milliseconds(dist(gen)));
}
}

int main() {
cout << "=== Prioridad a Lectores ===" << endl;

vector<thread> lectores;
vector<thread> escritores;

for (int i = 1; i <= 5; ++i)
lectores.emplace_back(lector, i);

for (int i = 1; i <= 2; ++i)
escritores.emplace_back(escritor, i);

for (auto &t : lectores) t.join();
for (auto &t : escritores) t.join();

return 0;
}
