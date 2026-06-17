#include "TempoCalculator.h"

float TempoCalculator::calcBPM(unsigned long intervalMs) {
    if (intervalMs == 0) return 0;
    return 60000.0f / (float)intervalMs;
}

void TempoCalculator::addBPM(float bpm) {
    // 現実的な範囲外の値はノイズとして除外
    if (bpm < 30 || bpm > 300) return;
    intervalBuf[bufIdx] = bpm;
    bufIdx = (bufIdx + 1) % BPM_BUF_SIZE;
    if (count < BPM_BUF_SIZE) count++;
}

float TempoCalculator::averageBPM() {
    if (count == 0) return 0;
    float sum = 0;
    for (int i = 0; i < count; i++) sum += intervalBuf[i];
    return sum / (float)count;
}

int TempoCalculator::convertLevel(float bpm) {
    if (bpm >= BPM_FAST_THRESH) return 3;  // Fast
    if (bpm >= BPM_SLOW_THRESH) return 2;  // Normal
    return 1;                               // Slow
}
