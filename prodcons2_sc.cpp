// -----------------------------------------------------------------------------
//
// Sistemas concurrentes y Distribuidos.
// Seminario 2. Introducción a los monitores en C++11.
//
// archivo: prodcons_2.cpp
//
// -----------------------------------------------------------------------------


#include <iostream>
#include <iomanip>
#include <cassert>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include "scd.h"

using namespace std ;
using namespace scd ;

constexpr int
        num_items  = 40,     // número de items a producir/consumir
    num_productores = 4,
    num_consumidores = 4;

mutex
        mtx ;                 // mutex de escritura en pantalla
unsigned
        cont_prod[num_items], // contadores de verificación: producidos
        cont_cons[num_items], // contadores de verificación: consumidos
        producidos[num_productores];  // contador de items producidos por cada productos

//**********************************************************************
// funciones comunes a las dos soluciones (fifo y lifo)
//----------------------------------------------------------------------

int producir_dato(int num_hebra)
{
    if (producidos[num_hebra] < (num_items/num_productores)) {
        static int contador = 0;
        this_thread::sleep_for(chrono::milliseconds(aleatorio<20, 100>()));
        mtx.lock();
        cout << "producido: " << contador << endl << flush;
        producidos[num_hebra]++;
        mtx.unlock();
        cont_prod[contador]++;
        return contador++;
    }
}
//----------------------------------------------------------------------

void consumir_dato( unsigned dato )
{
    if ( num_items <= dato )
    {
        cout << " dato === " << dato << ", num_items == " << num_items << endl ;
        assert( dato < num_items );
    }
    cont_cons[dato] ++ ;
    this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ));
    mtx.lock();
    cout << "                  consumido: " << dato << endl ;
    mtx.unlock();
}
//----------------------------------------------------------------------

void ini_contadores()
{
    for( unsigned i = 0 ; i < num_items ; i++ )
    {  cont_prod[i] = 0 ;
        cont_cons[i] = 0 ;
    }
}

//----------------------------------------------------------------------

void test_contadores()
{
    bool ok = true ;
    cout << "comprobando contadores ...." << flush ;

    for( unsigned i = 0 ; i < num_items ; i++ )
    {
        if ( cont_prod[i] != 1 )
        {
            cout << "error: valor " << i << " producido " << cont_prod[i] << " veces." << endl ;
            ok = false ;
        }
        if ( cont_cons[i] != 1 )
        {
            cout << "error: valor " << i << " consumido " << cont_cons[i] << " veces" << endl ;
            ok = false ;
        }
    }
    if (ok)
        cout << endl << flush << "solución (aparentemente) correcta." << endl << flush ;
}

// *****************************************************************************
// clase para monitor buffer, version LIFO, semántica SC, un prod. y un cons.

class ProdCons2SC
{
private:
    static const int           // constantes:
    num_celdas_total = 10;   //  núm. de entradas del buffer
    int                        // variables permanentes
    buffer[num_celdas_total],//  buffer de tamaño fijo, con los datos
    primera_libre ,          //  indice de celda de la próxima inserción
    primera_ocupada,
    celdas_ocupadas;

    mutex
            cerrojo_monitor ;        // cerrojo del monitor
    condition_variable         // colas condicion:
    ocupadas,                //  cola donde espera el consumidor (n>0)
    libres ;                 //  cola donde espera el productor  (n<num_celdas_total)

public:                    // constructor y métodos públicos
    ProdCons2SC(  ) ;           // constructor
    int  leer();                // extraer un valor (sentencia L) (consumidor)
    void escribir( int valor ); // insertar un valor (sentencia E) (productor)
} ;
// -----------------------------------------------------------------------------

ProdCons2SC::ProdCons2SC(  )
{
    primera_libre = 0 ;
    primera_ocupada = 0 ;
    celdas_ocupadas = 0 ;
}
// -----------------------------------------------------------------------------
// función llamada por el consumidor para extraer un dato

int ProdCons2SC::leer(  )
{
    // ganar la exclusión mutua del monitor con una guarda
    unique_lock<mutex> guarda( cerrojo_monitor );

    // esperar bloqueado hasta que 0 < num_celdas_ocupadas
    while ( celdas_ocupadas == 0 )
        ocupadas.wait( guarda );

    // hacer la operación de lectura, actualizando estado del monitor
    assert( 0 < celdas_ocupadas );
    const int valor = buffer[primera_ocupada];
    primera_ocupada = (primera_ocupada + 1) % num_celdas_total;
    celdas_ocupadas--;


    // señalar al productor que hay un hueco libre, por si está esperando
    libres.notify_one();

    // devolver valor
    return valor ;
}
// -----------------------------------------------------------------------------

void ProdCons2SC::escribir( int valor )
{
    // ganar la exclusión mutua del monitor con una guarda
    unique_lock<mutex> guarda( cerrojo_monitor );

    // esperar bloqueado hasta que num_celdas_ocupadas < num_celdas_total
    while ( celdas_ocupadas == num_celdas_total )
        libres.wait( guarda );

    //cout << "escribir: ocup == " << num_celdas_ocupadas << ", total == " << num_celdas_total << endl ;
    assert( celdas_ocupadas < num_celdas_total );

    // hacer la operación de inserción, actualizando estado del monitor
    buffer[primera_libre] = valor ;
    primera_libre = (primera_libre + 1) % num_celdas_total;
    celdas_ocupadas++ ;

    // señalar al consumidor que ya hay una celda ocupada (por si esta esperando)
    ocupadas.notify_one();
}
// *****************************************************************************
// funciones de hebras

void funcion_hebra_productora( ProdCons2SC * monitor, int num_hebra )
{
    for( unsigned i = 0 ; i < num_items/num_productores ; i++ )
    {
        int valor = producir_dato(num_hebra) ;
        monitor->escribir( valor );
    }
}
// -----------------------------------------------------------------------------

void funcion_hebra_consumidora( ProdCons2SC * monitor, int num_hebra)
{
    for( unsigned i = 0 ; i < num_items/num_consumidores ; i++ )
    {
        int valor = monitor->leer();
        consumir_dato( valor ) ;
    }
}
// -----------------------------------------------------------------------------

int main()
{
    cout << "-------------------------------------------------------------------------------" << endl
         << "Problema de los productores-consumidores (1 prod/cons, Monitor SC, buffer FIFO). " << endl
         << "-------------------------------------------------------------------------------" << endl
         << flush ;

    ProdCons2SC monitor ;

    thread hebra_productora[num_productores];
    thread hebra_consumidores[num_consumidores];

    //Primero inicializamos el buffer de los producidos

    for (int i = 0; i < num_productores; i++)
        producidos[i] = 0;

    for (int i = 0; i < num_productores; i++)
        hebra_productora[i] = thread (funcion_hebra_productora, &monitor, i);

    for (int i = 0; i < num_consumidores; i++)
        hebra_consumidores[i] = thread (funcion_hebra_consumidora, &monitor, i);

    for (int i = 0; i < num_productores; i++)
        hebra_productora[i].join();

    for (int i = 0; i < num_consumidores; i++)
        hebra_consumidores[i].join();


    // comprobar que cada item se ha producido y consumido exactamente una vez
    test_contadores() ;
}
