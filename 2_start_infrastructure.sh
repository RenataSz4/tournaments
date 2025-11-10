#!/bin/bash
# Script para iniciar PostgreSQL y ActiveMQ

set -e

echo "========================================"
echo "PASO 2: INICIANDO INFRAESTRUCTURA"
echo "========================================"

# Crear red si no existe
echo "Creando red 'development'..."
podman network create development 2>/dev/null || echo "Red ya existe"

# Iniciar PostgreSQL
echo ""
echo "Iniciando PostgreSQL..."
podman stop tournament_db 2>/dev/null || true
podman rm tournament_db 2>/dev/null || true

podman run -d --replace \
  --name=tournament_db \
  --network development \
  -e POSTGRES_PASSWORD=password \
  -p 5432:5432 \
  postgres:17.6-alpine3.22

echo "Esperando a que PostgreSQL inicie (5 segundos)..."
sleep 5

# Inicializar base de datos
echo "Inicializando base de datos..."
podman exec -i tournament_db psql -U postgres -d postgres < /tournaments/database/db_script.sql

# Verificar base de datos
echo ""
echo "Verificando tablas creadas:"
podman exec -it tournament_db psql -U postgres -d tournament_db -c "\dt"

# Iniciar ActiveMQ
echo ""
echo "Iniciando ActiveMQ..."
podman stop artemis 2>/dev/null || true
podman rm artemis 2>/dev/null || true

podman run -d --replace \
  --name artemis \
  --network development \
  -p 61616:61616 \
  -p 8161:8161 \
  apache/activemq-classic:6.1.7

echo "Esperando a que ActiveMQ inicie (10 segundos)..."
sleep 10

echo ""
echo "✅ Infraestructura lista!"
echo ""
echo "Servicios corriendo:"
podman ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"
echo ""
echo "ActiveMQ Console: http://localhost:8161/admin (admin/admin)"
echo ""
