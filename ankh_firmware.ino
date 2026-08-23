/**
 * @file ankh_firmware.ino
 * @brief 6-Axis Electromagnetic Trap Control, Particle Emission Management,
 *        and Deep-Voxel Tomography Processing
 * @status: SYSTEM FREEZE ACTIVE -- HARDWARE DEPENDENCY LOCK ENFORCED
 * @3-6-9: 4" dish, 12" sleeves (1.5"/6.0"/4.5"), 6-axis trap, 70.47 Hz clock
 */

#include <Arduino.h>
#include <SPI.h>

// ============================================================================
// 3-6-9 HARMONIC CONSTANTS
// ============================================================================
#define BASE_CLOCK_HZ 70.47f
#define MODULATION_CLOCK_HZ 634.23f      // 9 × 70.47 Hz
#define NUM_TRAP_AXES 6
#define SLEEVE_LENGTH_INCHES 12.0f
#define FRONT_BUFFER_INCHES 1.5f
#define FLOTATION_CORRIDOR_INCHES 6.0f
#define REAR_ACCUMULATOR_INCHES 4.5f
#define DISH_DIAMETER_INCHES 4.0f
#define GIZA_ANGLE 51.84f
#define PHASE_MAX 16384                  // 14-bit DDS resolution

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
// AD9959 DDS SPI Bus (6-Axis Trap Control)
#define DDS_CS    5
#define DDS_SCK   18
#define DDS_SDI   23
#define DDS_SDO   19
#define DDS_UPDATE 4
#define DDS_RESET 2

// Trap Axis Control Lines (6 axes)
const int trap_pins[NUM_TRAP_AXES] = {8, 9, 10, 11, 12, 13};

// Sleeve Control Lines (3 sleeves: Top, Side1, Side2)
const int sleeve_pins[3] = {14, 15, 16};

// Particle Emission Control
#define PARTICLE_EMITTER 17
#define PARTICLE_DETECTOR 18

// ============================================================================
// GLOBAL SYSTEM STATE REGISTERS
// ============================================================================
volatile uint16_t ankh_trap_field[NUM_TRAP_AXES];
volatile uint16_t ankh_sleeve_phase[3];
volatile uint8_t ankh_trap_status = 0;        // 0=Idle, 1=Trapping, 2=Scanning, 3=Release
volatile float ankh_coherence = 1.0f;
volatile uint32_t ankh_particle_emission_freq = 5708;  // 9 × modulation clock
volatile uint32_t ankh_clock_ticks = 0;

// ============================================================================
// HARDWARE TIMER INTERRUPT (70.47 Hz Base Clock)
// ============================================================================
hw_timer_t * ankh_timer = NULL;

void IRAM_ATTR ankh_clock_interrupt() {
    ankh_clock_ticks++;
    
    // 1. Update trap field phases for all 6 axes
    for (int axis = 0; axis < NUM_TRAP_AXES; axis++) {
        float axis_angle = axis * (2 * M_PI / NUM_TRAP_AXES);
        uint16_t phase = (uint16_t)((axis_angle / (2 * M_PI)) * PHASE_MAX) & 0x3FFF;
        ankh_trap_field[axis] = phase;
    }
    
    // 2. Update sleeve phases (Top/Side1/Side2)
    for (int sleeve = 0; sleeve < 3; sleeve++) {
        float sleeve_angle = sleeve * (2 * M_PI / 3);
        uint16_t phase = (uint16_t)((sleeve_angle / (2 * M_PI)) * PHASE_MAX) & 0x3FFF;
        ankh_sleeve_phase[sleeve] = phase;
