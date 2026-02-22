import serial
import csv
import sys

PORT   = "/dev/ttyUSB0"
BAUD   = 115200
OUTPUT = "dados_mpu6050.csv"

HEADER = ["timestamp_us","ax_g","ay_g","az_g","gx_dps","gy_dps","gz_dps","temp_c","dropped"]

def main():
    with serial.Serial(PORT, BAUD, timeout=2) as ser, \
         open(OUTPUT, "w", newline="") as f:

        writer = csv.writer(f)
        writer.writerow(HEADER)   
        f.flush()

        print(f"Lendo de {PORT}... Ctrl+C para parar.")
        while True:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            if line.startswith(("I ", "W ", "E ", "D ")):
                print(f"[LOG] {line}")
                continue

            # Ignora cabeçalho duplicado vindo do ESP32
            if line.startswith("timestamp_us"):
                continue

            fields = line.split(",")
            if len(fields) != 9:
                continue

            writer.writerow(fields)
            f.flush()

            ts, ax, ay, az, gx, gy, gz, temp, drop = fields
            try:
                
                ts_sec = int(ts) / 1e6
                ax_val = float(ax)
                ay_val = float(ay)
                az_val = float(az)
                gx_val = float(gx)
                gy_val = float(gy)
                gz_val = float(gz)
                temp_val = float(temp)
                
                print(f"[{ts_sec:10.3f}s] "
                      f"A: {ax_val:+.3f} {ay_val:+.3f} {az_val:+.3f} g | "
                      f"G: {gx_val:+.3f} {gy_val:+.3f} {gz_val:+.3f} °/s | "
                      f"{temp_val:.1f}°C drop={drop}")
            
            except ValueError as e:
                print(f"[ERRO] Linha inválida: {line} ({e})")
                continue

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nEncerrado.")

