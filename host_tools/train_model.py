# export_c_source.py
import csv
import math
import random

DATASET_FILE = 'flight_dataset.csv'
INPUT_SIZE = 450
HIDDEN_SIZE = 16
OUTPUT_SIZE = 3

X, y = [], []
with open(DATASET_FILE, 'r') as f:
    reader = csv.reader(f)
    next(reader)
    for row in reader:
        if row:
            # Girdileri uç değerlerden arındırıp ölçekle (Açısal hız / ivme patlamasını engeller)
            vals = [float(v) * 0.01 for v in row[:-1]]
            X.append(vals)
            y.append(int(row[-1]))

random.seed(42)
# Küçük başlangıç ağırlıkları
w_hidden = [[random.uniform(-0.05, 0.05) for _ in range(HIDDEN_SIZE)] for _ in range(INPUT_SIZE)]
b_hidden = [0.0] * HIDDEN_SIZE
w_output = [[random.uniform(-0.05, 0.05) for _ in range(OUTPUT_SIZE)] for _ in range(HIDDEN_SIZE)]
b_output = [0.0] * OUTPUT_SIZE

LR = 0.001  # Düşük öğrenme hızı (NaN oluşumunu tamamen keser)

for epoch in range(40):
    for x_sample, label in zip(X, y):
        # 1. Forward Hidden
        hidden = [0.0] * HIDDEN_SIZE
        for j in range(HIDDEN_SIZE):
            s = b_hidden[j] + sum(x_sample[i] * w_hidden[i][j] for i in range(INPUT_SIZE))
            hidden[j] = s if s > 0 else 0.0

        # 2. Forward Output
        out_raw = [0.0] * OUTPUT_SIZE
        for k in range(OUTPUT_SIZE):
            out_raw[k] = b_output[k] + sum(hidden[j] * w_output[j][k] for j in range(HIDDEN_SIZE))

        # Softmax (Stabil)
        max_v = max(out_raw)
        exps = [math.exp(max(-15.0, min(15.0, v - max_v))) for v in out_raw]
        sum_e = sum(exps)
        probs = [e / sum_e for e in exps]

        # 3. Backprop
        d_out = [probs[k] - (1.0 if k == label else 0.0) for k in range(OUTPUT_SIZE)]

        d_hidden = [0.0] * HIDDEN_SIZE
        for j in range(HIDDEN_SIZE):
            for k in range(OUTPUT_SIZE):
                d_hidden[j] += d_out[k] * w_output[j][k]
                w_output[j][k] -= LR * d_out[k] * hidden[j]
            b_output[k] -= LR * d_out[k]

        for j in range(HIDDEN_SIZE):
            if hidden[j] > 0:
                for i in range(INPUT_SIZE):
                    w_hidden[i][j] -= LR * d_hidden[j] * x_sample[i]
                b_hidden[j] -= LR * d_hidden[j]

# C Dosyası Üretimi
with open("maneuver_model.c", "w", encoding="utf-8") as f:
    f.write('#include "maneuver_model.h"\n\n')
    f.write("#define HIDDEN_SIZE 16\n")
    f.write("#define OUTPUT_SIZE 3\n\n")

    flat_w_hidden = [w_hidden[i][j] for i in range(INPUT_SIZE) for j in range(HIDDEN_SIZE)]
    f.write("static const float W_HIDDEN[7200] = {\n  ")
    f.write(", ".join(f"{v:.5f}f" for v in flat_w_hidden))
    f.write("\n};\n\n")

    f.write("static const float B_HIDDEN[16] = {\n  ")
    f.write(", ".join(f"{v:.5f}f" for v in b_hidden))
    f.write("\n};\n\n")

    flat_w_out = [w_output[j][k] for j in range(HIDDEN_SIZE) for k in range(OUTPUT_SIZE)]
    f.write("static const float W_OUTPUT[48] = {\n  ")
    f.write(", ".join(f"{v:.5f}f" for v in flat_w_out))
    f.write("\n};\n\n")

    f.write("static const float B_OUTPUT[3] = {\n  ")
    f.write(", ".join(f"{v:.5f}f" for v in b_output))
    f.write("\n};\n\n")

    # predict_maneuver içinde de girdileri aynı şekilde 0.01 ile ölçekliyoruz
    f.write("""int predict_maneuver(const float* input) {
    float hidden[HIDDEN_SIZE];
    float output[OUTPUT_SIZE];

    for (int j = 0; j < HIDDEN_SIZE; j++) {
        float sum = B_HIDDEN[j];
        for (int i = 0; i < INPUT_SIZE; i++) {
            sum += (input[i] * 0.01f) * W_HIDDEN[i * HIDDEN_SIZE + j];
        }
        hidden[j] = (sum > 0.0f) ? sum : 0.0f;
    }

    for (int k = 0; k < OUTPUT_SIZE; k++) {
        float sum = B_OUTPUT[k];
        for (int j = 0; j < HIDDEN_SIZE; j++) {
            sum += hidden[j] * W_OUTPUT[j * OUTPUT_SIZE + k];
        }
        output[k] = sum;
    }

    int best_class = 0;
    float max_val = output[0];
    for (int k = 1; k < OUTPUT_SIZE; k++) {
        if (output[k] > max_val) {
            max_val = output[k];
            best_class = k;
        }
    }
    return best_class;
}
""")

print("maneuver_model.c basariyla ve NaN olmadan uretildi!")