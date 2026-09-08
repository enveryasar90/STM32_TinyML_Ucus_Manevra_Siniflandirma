# STM32 TinyML Gerçek Zamanlı Uçuş Manevra Sınıflandırma ve 3D Simülatör

Bu proje; STM32F446RE mikrodenetleyici ve MPU6050 IMU sensörü kullanarak temel uçuş manevralarını (Düz Uçuş, Tono/Barrel Roll, Çember/Loop) doğrudan cihaz üzerinde (Edge AI) sınıflandıran ve eş zamanlı olarak Python tabanlı 3D OpenGL uçuş simülatöründe görselleştiren uçtan uca bir TinyML sistemidir.

---

### Sistem Mimarisi ve Çalışma Mantığı

1. **Veri Toplama ve Filtreleme:** MPU6050 sensöründen I2C ile 50 Hz hızında 6 eksen ham veri okunur. Titreşim ve kaymaları önlemek için Tamamlayıcı Filtre (Complementary Filter) ile açısal yönelim (Roll/Pitch) hesaplanır.
2. **Kayan Pencere (Sliding Window):** 75 örnekten oluşan zaman penceresi ($1.5\text{ sn} \times 6\text{ kanal} = 450\text{ özellik}$) sürekli güncellenir.
3. **Uçta Çıkarım (On-Device Inference):** Harici kütüphane bağımlılığı olmadan, saf C dilinde yazılmış Çok Katmanlı Algılayıcı (MLP) modeli mikrodenetleyici üzerinde mikro saniyeler içinde karara varır.
4. **Hibrit Filtre:** Durgun anlarda yapay zeka sıçramalarını önlemek için fiziksel açısal hız eşiği ($|\vec{\omega}| < 25^\circ/\text{s}$) uygulanır.
5. **Görselleştirme ve Donanım Çıkışı:** Tespit edilen manevra GPIO LED'leri ile gösterilirken, UART telemetrisi ile PyOpenGL simülatöründeki 3D uçak modeli anlık yönlendirilir.

---

### Donanım Bağlantıları

* **MPU6050 SCL:** Nucleo `PB8` (D15)
* **MPU6050 SDA:** Nucleo `PB9` (D14)
* **Durum LED'leri:**
  * `D2` (`PA10`): Düz Uçuş (Level Flight)
  * `D3` (`PB3`): Tono (Barrel Roll)
  * `D4` (`PB5`): Çember (Inside Loop)
* **Haberleşme:** ST-Link Sanal COM Port üzerinden USART2 (115200 Baud).

---

### Proje Yapısı

```
├── firmware/
│   ├── Core/
│   │   ├── Inc/
│   │   │   ├── main.h
│   │   │   └── maneuver_model.h      # Model fonksiyon prototipi
│   │   └── Src/
│   │       ├── main.c                # IMU okuma, filtreleme, UART ve ana döngü
│   │       └── maneuver_model.c      # Düzleştirilmiş ağırlıklar ve C çıkarım kodu
│   └── 3D_Manevra_YZ.ioc
├── host_tools/
│   ├── data_collector.py             # Telemetri veri toplama aracı
│   ├── train_model.py                # Sinir ağı eğitimi ve C kodu üretici
│   └── flight_sim.py                 # PyOpenGL 3D dijital ikiz simülatörü
├── data/
│   └── flight_dataset.csv            # Etiketli manevra veri seti
└── README.md

```
---

### Kurulum ve Çalıştırma

**1. Donanım Yazılımı (Firmware):**
* `firmware/` projesini STM32CubeIDE ile açın.
* Projeyi derleyip STM32F446RE kartınıza yükleyin.

**2. 3D Simülasyon Arayüzü:**
* Gerekli Python paketlerini yükleyin:
  ```bash
  pip install pygame PyOpenGL pyserial
* Kartın bağlı olduğu COM portunu flight_sim.py içinde ayarlayın ve çalıştırın:
  python host_tools/flight_sim.py
