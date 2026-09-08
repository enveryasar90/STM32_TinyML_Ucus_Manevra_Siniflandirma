import serial
import csv
import time
import os

SERIAL_PORT = 'COM4'
BAUD_RATE = 115200

# Toplanacak sınıflar
CLASSES = {
    '0': 'DUZ_UCUS',
    '1': 'BARREL_ROLL',
    '2': 'LOOP'
}

SAMPLE_WINDOW = 75  # 50 Hz ile 1.5 saniye (75 satır veri)
OUTPUT_FILE = 'flight_dataset.csv'

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.5)
        ser.reset_input_buffer()
        print(f"--> {SERIAL_PORT} baglandi!")
    except Exception as e:
        print(f"Baglanti hatasi: {e}")
        return

    # CSV dosyasını ve başlıkları hazırla
    file_exists = os.path.exists(OUTPUT_FILE)
    csv_file = open(OUTPUT_FILE, mode='a', newline='')
    writer = csv.writer(csv_file)
    
    if not file_exists:
        # Header: 6 sensör kanalı + etiket
        header = []
        for i in range(SAMPLE_WINDOW):
            header.extend([f"ax_{i}", f"ay_{i}", f"az_{i}", f"gx_{i}", f"gy_{i}", f"gz_{i}"])
        header.append("label")
        writer.writerow(header)
        csv_file.flush()

    print("\n--- MANEVRA VERI TOPLAYICI ---")
    print("0: DUZ UCUS kaydet")
    print("1: BARREL ROLL kaydet")
    print("2: LOOP kaydet")
    print("q: Cikis")

    while True:
        cmd = input("\nKaydedilecek Manevra Secimi (0, 1, 2) ve ardindan ENTER: ").strip()
        if cmd == 'q':
            break
        if cmd not in CLASSES:
            print("Gecersiz secim!")
            continue

        label_name = CLASSES[cmd]
        label_id = int(cmd)

        print(f"\nHazirlan! 1 saniye icinde '{label_name}' basliyor...")
        time.sleep(1.0)
        ser.reset_input_buffer()
        print(">>> HAREKETI YAP! <<<")

        window_data = []
        collected_samples = 0

        while collected_samples < SAMPLE_WINDOW:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                parts = line.split(',')
                if len(parts) >= 8:
                    try:
                        # Gx, Gy, Gz, Ax, Ay, Az (İndeks 2-7)
                        gx = float(parts[2])
                        gy = float(parts[3])
                        gz = float(parts[4])
                        ax = float(parts[5])
                        ay = float(parts[6])
                        az = float(parts[7])
                        
                        window_data.extend([ax, ay, az, gx, gy, gz])
                        collected_samples += 1
                    except ValueError:
                        continue

        # Etiketi ekle ve CSV'ye yaz
        window_data.append(label_id)
        writer.writerow(window_data)
        csv_file.flush()
        print(f"Tamamlandi! '{label_name}' basariyla kaydedildi.")

    csv_file.close()
    ser.close()
    print("\nVeri toplama bitti.")

if __name__ == '__main__':
    main()