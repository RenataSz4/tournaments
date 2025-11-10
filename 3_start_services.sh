#!/bin/bash
# Script para iniciar los servicios (API y Consumer)

set -e

echo "========================================"
echo "PASO 3: INICIANDO SERVICIOS"
echo "========================================"

cd /tournaments

# Verificar que existan los ejecutables
if [ ! -f "build/tournament_services/tournament_services" ]; then
    echo "❌ ERROR: tournament_services no está compilado"
    echo "Ejecuta primero: ./1_compile.sh"
    exit 1
fi

if [ ! -f "build/tournament_consumer/tournament_consumer" ]; then
    echo "❌ ERROR: tournament_consumer no está compilado"
    echo "Ejecuta primero: ./1_compile.sh"
    exit 1
fi

# Matar procesos anteriores si existen
echo "Deteniendo servicios anteriores..."
pkill -f tournament_services || true
pkill -f tournament_consumer || true
sleep 2

# Copiar archivos de configuración si no existen
if [ ! -f "build/tournament_services/configuration.json" ]; then
    echo "Copiando configuración de services..."
    cp tournament_services/configuration.json build/tournament_services/
fi

if [ ! -f "build/tournament_consumer/configuration.json" ]; then
    echo "Copiando configuración de consumer..."
    cp tournament_consumer/configuration.json build/tournament_consumer/
fi

# Iniciar tournament_services (API REST)
echo ""
echo "Iniciando tournament_services (API REST) en puerto 8080..."
cd build/tournament_services
./tournament_services > ../../logs_services.txt 2>&1 &
SERVICES_PID=$!
cd ../..

sleep 2

# Verificar que inició correctamente
if ps -p $SERVICES_PID > /dev/null; then
    echo "✅ tournament_services iniciado (PID: $SERVICES_PID)"
else
    echo "❌ ERROR: tournament_services falló al iniciar"
    echo "Ver logs: tail -f logs_services.txt"
    exit 1
fi

# Iniciar tournament_consumer (Event Listener)
echo ""
echo "Iniciando tournament_consumer (Event Listener)..."
cd build/tournament_consumer
./tournament_consumer > ../../logs_consumer.txt 2>&1 &
CONSUMER_PID=$!
cd ../..

sleep 2

# Verificar que inició correctamente
if ps -p $CONSUMER_PID > /dev/null; then
    echo "✅ tournament_consumer iniciado (PID: $CONSUMER_PID)"
else
    echo "❌ ERROR: tournament_consumer falló al iniciar"
    echo "Ver logs: tail -f logs_consumer.txt"
    exit 1
fi

echo ""
echo "========================================"
echo "✅ TODOS LOS SERVICIOS CORRIENDO"
echo "========================================"
echo ""
echo "API REST:        http://localhost:8080"
echo "ActiveMQ Admin:  http://localhost:8161/admin"
echo ""
echo "PIDs:"
echo "  - tournament_services: $SERVICES_PID"
echo "  - tournament_consumer: $CONSUMER_PID"
echo ""
echo "Logs:"
echo "  - Services: tail -f logs_services.txt"
echo "  - Consumer: tail -f logs_consumer.txt"
echo ""
echo "Para detener: pkill -f tournament_services && pkill -f tournament_consumer"
echo ""
