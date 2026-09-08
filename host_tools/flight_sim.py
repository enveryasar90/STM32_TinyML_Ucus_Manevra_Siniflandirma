import pygame
from pygame.locals import *
from OpenGL.GL import *
from OpenGL.GLU import *
import serial
import threading
import sys

SERIAL_PORT = 'COM4' 
BAUD_RATE = 115200

roll, pitch = 0.0, 0.0
maneuver_id = 0
maneuver_names = {
    0: "DUZ UCUS (LEVEL)",
    1: "BARREL ROLL (TONO)",
    2: "INSIDE LOOP (CEMBER)"
}

def serial_reader():
    global roll, pitch, maneuver_id
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        ser.reset_input_buffer()
        print(f"--> {SERIAL_PORT} baglandi, canli telemetri aliniyor...")

        while True:
            if ser.in_waiting > 500:
                ser.reset_input_buffer()

            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                parts = line.split(',')
                if len(parts) >= 9:
                    try:
                        roll = float(parts[0])
                        pitch = float(parts[1])
                        maneuver_id = int(parts[8])
                    except ValueError:
                        continue
    except Exception as e:
        print(f"Seri port hatasi: {e}")

threading.Thread(target=serial_reader, daemon=True).start()

def draw_aircraft():
    # Gövde (Mavi)
    glColor3f(0.1, 0.5, 0.9)
    glBegin(GL_TRIANGLES)
    glVertex3f(0.0, 0.0, 2.0)
    glVertex3f(-0.4, 0.0, -1.0)
    glVertex3f(0.4, 0.0, -1.0)
    glEnd()

    # Kanatlar (Beyaz)
    glColor3f(0.9, 0.9, 0.9)
    glBegin(GL_TRIANGLES)
    glVertex3f(0.0, 0.0, 0.6)
    glVertex3f(-2.5, 0.0, -0.8)
    glVertex3f(2.5, 0.0, -0.8)
    glEnd()

    # Dikey Kuyruk (Kırmızı)
    glColor3f(0.9, 0.1, 0.1)
    glBegin(GL_TRIANGLES)
    glVertex3f(0.0, 0.0, -0.5)
    glVertex3f(0.0, 0.8, -1.4)
    glVertex3f(0.0, 0.0, -1.4)
    glEnd()

def draw_text(text):
    pygame.display.set_caption(f"STM32 Edge AI | TESPIT EDILEN MANEVRA: {text}")

def main():
    pygame.init()
    display = (900, 650)
    pygame.display.set_mode(display, DOUBLEBUF | OPENGL)

    glMatrixMode(GL_PROJECTION)
    gluPerspective(45, (display[0] / display[1]), 0.1, 50.0)
    glMatrixMode(GL_MODELVIEW)
    glEnable(GL_DEPTH_TEST)

    clock = pygame.time.Clock()

    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()
        
        gluLookAt(0, 2.5, -6,  0, 0, 0,  0, 1, 0)

        glPushMatrix()
        glRotatef(pitch, 1, 0, 0)
        glRotatef(roll, 0, 0, 1)
        draw_aircraft()
        glPopMatrix()

        # Pencere başlığına anlık tespit edilen manevrayı bas
        text = maneuver_names.get(maneuver_id, "BILINMIYOR")
        draw_text(text)

        pygame.display.flip()
        clock.tick(60)

if __name__ == '__main__':
    main()