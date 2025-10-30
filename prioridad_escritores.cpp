#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <random>
using namespace std;

// Variables compartidas
int recurso = 0;
int lectores_activos = 0;
int escritores_esperando = 0;
bool escribiendo = false;

// Sincronización
mutex mtx;
condition_variable puede_leer;
condition_variable puede_escribir;

void lector(int id) {
random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> dist(100, 300);

while (true) {
unique_lock<mutex> lock(mtx);
// Esperar si hay escritores esperando o escribiendo
while (escritores_esperando > 0 || escribiendo)
puede_leer.wait(lock);

lectores_activos++;
lock.unlock();

// ---- Sección crítica (lectura) ----
cout << "👀 Lector " << id << " está leyendo el valor: " << recurso << endl;
this_thread::sleep_for(chrono::milliseconds(dist(gen)));

lock.lock();
lectores_activos--;
if (lectores_activos == 0)
puede_escribir.notify_one();
lock.unlock();

this_thread::sleep_for(chrono::milliseconds(dist(gen)));
}
}

void escritor(int id) {
random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> dist(200, 500);

while (true) {
unique_lock<mutex> lock(mtx);
escritores_esperando++;
// Esperar si hay lectores activos o ya hay otro escritor escribiendo
while (lectores_activos > 0 || escribiendo)
puede_escribir.wait(lock);
escritores_esperando--;
escribiendo = true;
lock.unlock();

// ---- Sección crítica (escritura) ----
recurso++;
cout << "📝 Escritor " << id << " modificó el recurso a: " << recurso << endl;
this_thread::sleep_for(chrono::milliseconds(dist(gen)));

lock.lock();
escribiendo = false;
if (escritores_esperando > 0)
puede_escribir.notify_one(); // prioridad a otro escritor
else
puede_leer.notify_all(); // permitir leer a todos
lock.unlock();

this_thread::sleep_for(chrono::milliseconds(dist(gen)));
}
}

int main() {
cout << "=== Prioridad a Escritores ===" << endl;

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
