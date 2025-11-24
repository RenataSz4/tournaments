from typing import Any
from locust import HttpUser, task, between
import json
import uuid
import random
import time

MUNDIAL_GRUPOS = 8
EQUIPOS_POR_GRUPO = 4
TOTAL_EQUIPOS = MUNDIAL_GRUPOS * EQUIPOS_POR_GRUPO


class UsuarioTorneo(HttpUser):

    wait_time = between(1, 3)

    def _extraer_id_de_location(self, location: str | None) -> str | None:
        # parsear el id del header location
        if not location:
            return None
        return location.rstrip("/").split("/")[-1]

    def crear_equipos(self, cantidad: int):
        # crear n equipos y retornar sus ids
        equipo_ids = []
        for _ in range(cantidad):
            equipo_data = {"name": f"Team {uuid.uuid4()}"}
            with self.client.post(
                "/teams",
                json=equipo_data,
                catch_response=True,
                name="POST /teams"
            ) as response:
                if response.status_code in (200, 201):
                    location = response.headers.get("Location") or response.headers.get("location")
                    equipo_id = self._extraer_id_de_location(location)
                    if equipo_id:
                        equipo_ids.append(equipo_id)
                    else:
                        response.failure("Missing team ID in Location header")
                else:
                    response.failure(f"Team creation failed: {response.status_code}")
        return equipo_ids

    def crear_torneo(self) -> str | None:
        # crear un torneo mundial y retornar su id
        torneo_data = {
            "name": f"Mundial {uuid.uuid4()}",
            "format": {
                "numberOfGroups": MUNDIAL_GRUPOS,
                "maxTeamsPerGroup": EQUIPOS_POR_GRUPO,
                "type": "MUNDIAL"
            }
        }
        with self.client.post(
            "/tournaments",
            json=torneo_data,
            catch_response=True,
            name="POST /tournaments"
        ) as response:
            if response.status_code in (200, 201):
                location = response.headers.get("Location") or response.headers.get("location")
                return self._extraer_id_de_location(location)
            else:
                response.failure(f"Tournament creation failed: {response.status_code}")
                return None

    def crear_grupos_con_equipos(self, torneo_id: str, grupos_data: list[dict]):
        # crear grupos con sus equipos incluidos en el torneo
        grupo_ids = []
        for grupo_data in grupos_data:
            with self.client.post(
                f"/tournaments/{torneo_id}/groups",
                json=grupo_data,
                catch_response=True,
                name="POST /tournaments/{id}/groups"
            ) as response:
                if response.status_code in (200, 201):
                    location = response.headers.get("Location") or response.headers.get("location")
                    grupo_id = self._extraer_id_de_location(location)
                    if grupo_id:
                        grupo_ids.append(grupo_id)
                    else:
                        response.failure("Missing group ID in Location header")
                else:
                    response.failure(f"Group creation failed: {response.status_code}")
        return grupo_ids

    def obtener_partidos_pendientes(self, torneo_id: str):
        # obtener todos los partidos pendientes de un torneo
        partidos = []
        with self.client.get(
            f"/tournaments/{torneo_id}/matches",
            catch_response=True,
            name="GET /tournaments/{id}/matches"
        ) as response:
            if response.status_code == 200:
                try:
                    todos_partidos = response.json()
                    # filtrar partidos sin score (pendientes)
                    partidos = [m for m in todos_partidos if 'score' not in m or m.get('score') is None]
                except json.JSONDecodeError:
                    response.failure("Invalid JSON response")
            else:
                response.failure(f"Fetch matches failed: {response.status_code}")
        return partidos

    def actualizar_marcador_partido(self, torneo_id: str, partido_id: str, goles_local: int, goles_visitante: int):
        # actualizar el marcador de un partido especifico
        score_data = {
            "score": {
                "home": goles_local,
                "visitor": goles_visitante
            }
        }
        with self.client.patch(
            f"/tournaments/{torneo_id}/matches/{partido_id}",
            json=score_data,
            catch_response=True,
            name="PATCH /tournaments/{id}/matches/{id}"
        ) as response:
            if response.status_code not in (200, 204):
                response.failure(f"Update match score failed: {response.status_code}")

    def simular_ronda(self, torneo_id: str, nombre_ronda: str, permitir_empates: bool = True):
        # simular una ronda completa scoring todos los partidos pendientes
        max_intentos = 10
        intento = 0

        while intento < max_intentos:
            partidos_pendientes = self.obtener_partidos_pendientes(torneo_id)

            if not partidos_pendientes:
                break

            for partido in partidos_pendientes:
                if 'id' not in partido:
                    continue

                goles_local = random.randint(0, 4)
                goles_visitante = random.randint(0, 4)

                # asegurar que no haya empates en knockout rounds
                if not permitir_empates:
                    while goles_local == goles_visitante:
                        goles_visitante = random.randint(0, 4)

                self.actualizar_marcador_partido(torneo_id, partido['id'], goles_local, goles_visitante)

            # pequeño delay antes de chequear mas partidos
            time.sleep(1)
            intento += 1

    @task
    def flujo_mundial(self):
        # workflow completo de mundial:
        # - crear 32 equipos
        # - crear torneo
        # - crear 8 grupos
        # - distribuir equipos (4 por grupo)
        # - scorear todos los partidos de fase de grupos
        # - scorear rondas de knockout (octavos, cuartos, semis, final)

        # crear equipos
        equipos = self.crear_equipos(TOTAL_EQUIPOS)
        if len(equipos) != TOTAL_EQUIPOS:
            return

        # crear torneo
        torneo_id = self.crear_torneo()
        if not torneo_id:
            return

        # crear grupos con equipos incluidos
        grupos_data = []
        for i in range(MUNDIAL_GRUPOS):
            inicio = i * EQUIPOS_POR_GRUPO
            fin = inicio + EQUIPOS_POR_GRUPO
            grupo_equipos = [{"id": equipo_id} for equipo_id in equipos[inicio:fin]]
            grupos_data.append({
                "name": f"Group {uuid.uuid4()}",
                "teams": grupo_equipos
            })

        grupos = self.crear_grupos_con_equipos(torneo_id, grupos_data)
        if len(grupos) != MUNDIAL_GRUPOS:
            return

        # simular fase de grupos (partidos regular - empates permitidos)
        self.simular_ronda(torneo_id, "regular", permitir_empates=True)

        # simular rondas de knockout (sin empates permitidos)
        # octavos
        self.simular_ronda(torneo_id, "eighths", permitir_empates=False)

        # cuartos
        self.simular_ronda(torneo_id, "quarters", permitir_empates=False)

        # semis
        self.simular_ronda(torneo_id, "semis", permitir_empates=False)

        # final
        self.simular_ronda(torneo_id, "final", permitir_empates=False)


class UsuarioLectura(HttpUser):
    # user class enfocado en operaciones de lectura para testear
    # la distribucion del load balancer a traves de multiples nodos haproxy

    wait_time = between(0.5, 2)

    def on_start(self):
        # inicializar obteniendo torneos y equipos existentes
        self.torneo_ids = []
        self.equipo_ids = []

        # obtener torneos existentes
        response = self.client.get("/tournaments")
        if response.status_code == 200:
            try:
                torneos = response.json()
                self.torneo_ids = [t['id'] for t in torneos if 'id' in t]
            except:
                pass

        # obtener equipos existentes
        response = self.client.get("/teams")
        if response.status_code == 200:
            try:
                equipos = response.json()
                self.equipo_ids = [t['id'] for t in equipos if 'id' in t]
            except:
                pass

    @task(20)
    def leer_torneos(self):
        # leer todos los torneos
        with self.client.get("/tournaments", catch_response=True, name="GET /tournaments") as response:
            if response.status_code == 200:
                try:
                    torneos = response.json()
                    # actualizar cache local
                    self.torneo_ids = [t['id'] for t in torneos if 'id' in t]
                except:
                    response.failure("Invalid JSON")
            else:
                response.failure(f"Failed: {response.status_code}")

    @task(20)
    def leer_equipos(self):
        # leer todos los equipos
        with self.client.get("/teams", catch_response=True, name="GET /teams") as response:
            if response.status_code == 200:
                try:
                    equipos = response.json()
                    # actualizar cache local
                    self.equipo_ids = [t['id'] for t in equipos if 'id' in t]
                except:
                    response.failure("Invalid JSON")
            else:
                response.failure(f"Failed: {response.status_code}")

    @task(15)
    def leer_grupos_de_torneo(self):
        # leer grupos de un torneo especifico
        if not self.torneo_ids:
            return

        torneo_id = random.choice(self.torneo_ids)
        with self.client.get(
            f"/tournaments/{torneo_id}/groups",
            catch_response=True,
            name="GET /tournaments/{id}/groups"
        ) as response:
            if response.status_code != 200:
                response.failure(f"Failed: {response.status_code}")

    @task(15)
    def leer_partidos_de_torneo(self):
        # leer partidos de un torneo especifico
        if not self.torneo_ids:
            return

        torneo_id = random.choice(self.torneo_ids)
        with self.client.get(
            f"/tournaments/{torneo_id}/matches",
            catch_response=True,
            name="GET /tournaments/{id}/matches"
        ) as response:
            if response.status_code != 200:
                response.failure(f"Failed: {response.status_code}")

    @task(10)
    def chequeo_salud(self):
        # chequear salud del servicio
        with self.client.get("/health", catch_response=True, name="GET /health") as response:
            if response.status_code != 200:
                response.failure(f"Health check failed: {response.status_code}")


class UsuarioEscritura(HttpUser):
    # user class enfocado en operaciones de escritura para stress test del sistema

    wait_time = between(1, 3)

    def on_start(self):
        # inicializar state
        self.torneo_ids = []
        self.grupo_ids = []
        self.partido_ids = []

    @task(10)
    def crear_equipo(self):
        # crear un nuevo equipo
        equipo_data = {"name": f"Team {uuid.uuid4()}"}
        with self.client.post(
            "/teams",
            json=equipo_data,
            catch_response=True,
            name="POST /teams"
        ) as response:
            if response.status_code not in (200, 201):
                response.failure(f"Failed: {response.status_code}")

    @task(8)
    def crear_torneo(self):
        # crear un nuevo torneo mundial
        torneo_data = {
            "name": f"Mundial {uuid.uuid4()}",
            "format": {
                "numberOfGroups": MUNDIAL_GRUPOS,
                "maxTeamsPerGroup": EQUIPOS_POR_GRUPO,
                "type": "MUNDIAL"
            }
        }
        with self.client.post(
            "/tournaments",
            json=torneo_data,
            catch_response=True,
            name="POST /tournaments"
        ) as response:
            if response.status_code in (200, 201):
                location = response.headers.get("Location") or response.headers.get("location")
                if location:
                    torneo_id = location.rstrip("/").split("/")[-1]
                    self.torneo_ids.append(torneo_id)
            else:
                response.failure(f"Failed: {response.status_code}")

    @task(6)
    def crear_grupo(self):
        # crear un nuevo grupo en un torneo (sin equipos para testing simple)
        if not self.torneo_ids:
            return

        torneo_id = random.choice(self.torneo_ids)
        # crear grupo vacio o con equipos random existentes
        grupo_data = {"name": f"Group {uuid.uuid4()}", "teams": []}

        with self.client.post(
            f"/tournaments/{torneo_id}/groups",
            json=grupo_data,
            catch_response=True,
            name="POST /tournaments/{id}/groups"
        ) as response:
            if response.status_code in (200, 201):
                location = response.headers.get("Location") or response.headers.get("location")
                if location:
                    grupo_id = location.rstrip("/").split("/")[-1]
                    self.grupo_ids.append((torneo_id, grupo_id))
            else:
                response.failure(f"Failed: {response.status_code}")

    @task(4)
    def actualizar_marcador_partido(self):
        # actualizar el marcador de un partido
        if not self.torneo_ids:
            return

        torneo_id = random.choice(self.torneo_ids)

        # obtener partidos de este torneo
        response = self.client.get(f"/tournaments/{torneo_id}/matches")
        if response.status_code != 200:
            return

        try:
            partidos = response.json()
            if not partidos:
                return

            partido = random.choice(partidos)
            if 'id' not in partido:
                return

            score_data = {
                "score": {
                    "home": random.randint(0, 4),
                    "visitor": random.randint(0, 4)
                }
            }

            with self.client.patch(
                f"/tournaments/{torneo_id}/matches/{partido['id']}",
                json=score_data,
                catch_response=True,
                name="PATCH /tournaments/{id}/matches/{id}"
            ) as patch_response:
                if patch_response.status_code not in (200, 204):
                    patch_response.failure(f"Failed: {patch_response.status_code}")
        except:
            pass
