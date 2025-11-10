#!/bin/bash
# Script para compilar el proyecto

set -e

echo "========================================"
echo "PASO 1: COMPILANDO PROYECTO"
echo "========================================"

cd /tournaments

# Limpiar build anterior
echo "Limpiando build anterior..."
rm -rf build

# Configurar CMake
echo "Configurando CMake..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compilar
echo "Compilando con $(nproc) cores..."
cmake --build build -j$(nproc)

echo ""
echo "✅ Compilación completada!"
echo ""
echo "Ejecutables generados:"
ls -lh build/tournament_services/tournament_services 2>/dev/null || echo "⚠️ tournament_services NO compiló"
ls -lh build/tournament_consumer/tournament_consumer 2>/dev/null || echo "⚠️ tournament_consumer NO compiló"
echo ""
