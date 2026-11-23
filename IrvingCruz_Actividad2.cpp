#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <windows.h>
#include <sql.h>
#include <sqlext.h>

// Vinculamos la libreria nativa ODBC de Windows
#pragma comment(lib, "odbc32.lib")

using namespace std;

// Funcion auxiliar para mostrar mensajes detallados en caso de que falle ODBC
void mostrarErrorODBC(SQLSMALLINT tipoManejador, SQLHANDLE manejador) {
    SQLWCHAR sqlState[6];
    SQLWCHAR mensaje[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER errorNativo;
    SQLSMALLINT longitudTexto;
    SQLSMALLINT i = 1;

    while (SQLGetDiagRecW(tipoManejador, manejador, i, sqlState, &errorNativo, mensaje, SQL_MAX_MESSAGE_LENGTH, &longitudTexto) == SQL_SUCCESS) {
        wcout << L"[Error ODBC " << sqlState << L"]: " << mensaje << endl;
        i++;
    }
}

// Funcion para ejecutar instrucciones SQL directas (crear la base de datos y las tablas)
bool ejecutarSQL(SQLHDBC dbc, const wstring& consulta) {
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    SQLRETURN ret = SQLExecDirectW(stmt, (SQLWCHAR*)consulta.c_str(), SQL_NTS);
    bool exito = (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);

    if (!exito) {
        mostrarErrorODBC(SQL_HANDLE_STMT, stmt);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    return exito;
}

// Funcion para imprimir en pantalla una tabla vacia mostrando unicamente sus encabezados
void imprimirTablaVacia(SQLHDBC dbc, const string& nombreTabla) {
    SQLHSTMT stmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    // Consultamos la estructura de la tabla solicitada
    wstring consulta = L"SELECT * FROM " + wstring(nombreTabla.begin(), nombreTabla.end());
    SQLRETURN ret = SQLExecDirectW(stmt, (SQLWCHAR*)consulta.c_str(), SQL_NTS);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        cout << "No se pudo consultar la tabla: " << nombreTabla << endl;
        mostrarErrorODBC(SQL_HANDLE_STMT, stmt);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        return;
    }

    // Obtenemos el numero total de columnas de la tabla
    SQLSMALLINT numColumnas = 0;
    SQLNumResultCols(stmt, &numColumnas);

    vector<string> encabezados;
    vector<int> anchos;

    // Extraemos el nombre de cada columna desde los metadatos de SQL Server
    for (SQLSMALLINT i = 1; i <= numColumnas; i++) {
        SQLWCHAR nombreCol[128];
        SQLSMALLINT longNombre, tipoDato, decimales, permiteNulo;
        SQLULEN tamanoCol;

        SQLDescribeColW(stmt, i, nombreCol, 128, &longNombre, &tipoDato, &tamanoCol, &decimales, &permiteNulo);

        wstring ws(nombreCol);
        string colStr(ws.begin(), ws.end());
        encabezados.push_back(colStr);

        // Calculamos un ancho comodo para dibujar la celda en consola
        int ancho = static_cast<int>(colStr.length()) + 4;
        if (ancho < 12) ancho = 12;
        anchos.push_back(ancho);
    }

    // Funcion interna para dibujar las lineas horizontales de la tabla (+---+---+)
    auto dibujarLinea = [&]() {
        cout << "+";
        for (int ancho : anchos) {
            cout << string(ancho, '-') << "+";
        }
        cout << endl;
        };

    // Imprimimos el titulo y el formato visual de la tabla vacia
    cout << "\nTABLA: " << nombreTabla << endl;
    dibujarLinea();

    cout << "|";
    for (size_t i = 0; i < encabezados.size(); i++) {
        cout << " " << left << setw(anchos[i] - 1) << encabezados[i] << "|";
    }
    cout << endl;

    dibujarLinea();

    // Fila vacia representativa (como solicita el Paso 6 y Paso 8)
    cout << "|";
    for (size_t i = 0; i < encabezados.size(); i++) {
        cout << string(anchos[i], ' ') << "|";
    }
    cout << endl;

    dibujarLinea();

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
}

int main() {
    // Manejadores principales de entorno y conexion ODBC
    SQLHENV env = NULL;
    SQLHDBC dbc = NULL;
    SQLRETURN ret;

    cout << "Iniciando conexion con SQL Server Express..." << endl;

    // 1. Asignamos el manejador de entorno y configuramos ODBC 3.0
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);

    // 2. Asignamos el manejador de conexion
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    // 3. Cadena de conexion apuntando a tu instancia local .\SQLEXPRESS
    wstring cadenaConexion = L"DRIVER={SQL Server};SERVER=.\\SQLEXPRESS;DATABASE=master;Trusted_Connection=yes;";

    ret = SQLDriverConnectW(dbc, NULL, (SQLWCHAR*)cadenaConexion.c_str(), SQL_NTS, NULL, 0, NULL, SQL_DRIVER_COMPLETE);

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        cout << "Conexion exitosa al servidor .\\SQLEXPRESS!" << endl;

        // 4. Creamos la base de datos EmpresaUNI si aun no existe y la seleccionamos
        ejecutarSQL(dbc, L"IF NOT EXISTS (SELECT name FROM sys.databases WHERE name = 'EmpresaUNI') CREATE DATABASE EmpresaUNI;");
        ejecutarSQL(dbc, L"USE EmpresaUNI;");

        // 5. Creamos las 3 tablas del escenario (si no existen)
        wstring sqlCentros =
            L"IF OBJECT_ID('Centros', 'U') IS NULL "
            L"CREATE TABLE Centros ("
            L"NumeroCentro VARCHAR(10) PRIMARY KEY, "
            L"NombreCentro VARCHAR(100) NOT NULL, "
            L"Ciudad VARCHAR(50) NOT NULL);";

        wstring sqlEmpleados =
            L"IF OBJECT_ID('Empleados', 'U') IS NULL "
            L"CREATE TABLE Empleados ("
            L"NumeroEmpleado INT IDENTITY(1,1) PRIMARY KEY, "
            L"Nombre VARCHAR(50) NOT NULL, "
            L"ApellidoPaterno VARCHAR(50) NOT NULL, "
            L"ApellidoMaterno VARCHAR(50) NOT NULL, "
            L"FechaNacimiento DATE NOT NULL, "
            L"RFC VARCHAR(13) NOT NULL, "
            L"NumeroCentro VARCHAR(10) NOT NULL, "
            L"Puesto VARCHAR(50) NOT NULL, "
            L"DescripcionPuesto VARCHAR(150) NOT NULL, "
            L"EsDirectivo BIT NOT NULL, "
            L"CONSTRAINT FK_Empleados_Centros FOREIGN KEY (NumeroCentro) REFERENCES Centros(NumeroCentro));";

        wstring sqlDirectivos =
            L"IF OBJECT_ID('Directivos', 'U') IS NULL "
            L"CREATE TABLE Directivos ("
            L"NumeroEmpleado INT PRIMARY KEY, "
            L"CentroSupervisa VARCHAR(10) NOT NULL, "
            L"PrestacionCombustible BIT NOT NULL, "
            L"CONSTRAINT FK_Directivos_Empleados FOREIGN KEY (NumeroEmpleado) REFERENCES Empleados(NumeroEmpleado), "
            L"CONSTRAINT FK_Directivos_Centros FOREIGN KEY (CentroSupervisa) REFERENCES Centros(NumeroCentro));";

        ejecutarSQL(dbc, sqlCentros);
        ejecutarSQL(dbc, sqlEmpleados);
        ejecutarSQL(dbc, sqlDirectivos);

        cout << "Tablas creadas y verificadas en la base de datos EmpresaUNI." << endl;

        // 6. Imprimimos en consola las 3 tablas vacias con sus encabezados
        imprimirTablaVacia(dbc, "Centros");
        imprimirTablaVacia(dbc, "Empleados");
        imprimirTablaVacia(dbc, "Directivos");

        // 7. Cerramos la conexion de forma segura
        SQLDisconnect(dbc);
    }
    else {
        cout << "Error al conectar con SQL Server Express." << endl;
        mostrarErrorODBC(SQL_HANDLE_DBC, dbc);
    }

    // 8. Liberamos los manejadores de memoria
    if (dbc) SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    if (env) SQLFreeHandle(SQL_HANDLE_ENV, env);

    cout << "\nProceso finalizado. Presiona Enter para salir..." << endl;
    cin.get();
    return 0;
}
