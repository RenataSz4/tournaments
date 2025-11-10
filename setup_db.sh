#!/bin/bash
# Script para inicializar la base de datos

echo "Ejecutando script SQL en PostgreSQL..."
podman exec -i tournament_db psql -U postgres -d postgres < database/db_script.sql

echo ""
echo "Verificando que la base de datos se creó..."
podman exec -it tournament_db psql -U postgres -c "\l" | grep tournament_db

echo ""
echo "Listando tablas creadas..."
podman exec -it tournament_db psql -U postgres -d tournament_db -c "\dt"

echo ""
echo "Listando tipos ENUM creados..."
podman exec -it tournament_db psql -U postgres -d tournament_db -c "\dT"

echo ""
echo "¡Base de datos inicializada correctamente!"
