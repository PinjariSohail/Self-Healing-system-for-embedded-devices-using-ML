#include "model_data.h"
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

// ------------------------------
// TensorFlow Lite settings
// ------------------------------
#define TENSOR_ARENA_SIZE 20 * 1024
uint8_t tensor_arena[TENSOR_ARENA_SIZE];

const tflite::Model* model;
tflite::MicroInterpreter* interpreter;
TfLiteTensor* input;
TfLiteTensor* output;

// ------------------------------
// Fault & Recovery mappings
// ------------------------------
const char* fault_labels[] = {
  "F_NONE", "F_GAS", "F_MOTION", "F_MOTOR", "F_DISPLAY", "FAULT_TEST"
};

const char* recovery_labels[] = {
  "R_NONE", "R_RESET_SENSOR", "R_REINIT_DISPLAY", "R_RESTART_MOTOR", "R_SIMULATED"
};

// fault_type_encoded → recovery_type_encoded mapping
int fault_to_recovery[] = {
  0, // F_NONE → R_NONE
  1, // F_GAS → R_RESET_SENSOR
  1, // F_MOTION → R_RESET_SENSOR
  3, // F_MOTOR → R_RESTART_MOTOR
  2, // F_DISPLAY → R_REINIT_DISPLAY
  4  // FAULT_TEST → R_SIMULATED
};

// ------------------------------
// Recovery Action Functions
// ------------------------------
void recoveryAction(int recovery_type) {
  switch (recovery_type) {
    case 0: // R_NONE
      Serial.println("✅ No recovery needed.");
      break;
    case 1: // R_RESET_SENSOR
      Serial.println("🔄 Resetting sensor...");
      // Add code to reset your MQ2 or IR sensor here
      break;
    case 2: // R_REINIT_DISPLAY
      Serial.println("🔄 Reinitializing display...");
      // Add code to reset your OLED/LCD display here
      break;
    case 3: // R_RESTART_MOTOR
      Serial.println("🔄 Restarting motor...");
      // Add motor driver restart code here
      break;
    case 4: // R_SIMULATED
      Serial.println("🧪 Performing simulated recovery...");
      // Any simulated recovery process here
      break;
  }
}

// ------------------------------
// Setup
// ------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Fault Detection & Recovery Model");

  model = tflite::GetModel(model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("Model schema %d not equal to supported %d\n",
                  model->version(), TFLITE_SCHEMA_VERSION);
    while (1);
  }

  // Error reporter
  class ArduinoErrorReporter : public tflite::ErrorReporter {
   public:
    int Report(const char* format, va_list args) override {
      char buf[256];
      vsnprintf(buf, sizeof(buf), format, args);
      Serial.println(buf);
      return 0;
    }
  };
  static ArduinoErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;

  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, TENSOR_ARENA_SIZE, error_reporter, nullptr, nullptr
  );
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("AllocateTensors() failed");
    while (1);
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("✅ Setup complete, ready to predict and recover.");
}

// ------------------------------
// Loop
// ------------------------------
void loop() {
  // Fill input tensor with 0.0 to avoid garbage values
  for (int i = 0; i < input->bytes / sizeof(float); i++) {
    input->data.f[i] = 0.0;
  }

  // Replace with real sensor values
  float mq2_value = 950.0;         // same unit/range as training data
  float motion_state_encoded = 1;  // 1 = NO MOTION, 0 = MOTION
  float fault_dummy = 0;           // always 0 for prediction
  float recovery_dummy = 0;        // always 0 for prediction

  // Assign features in same order as training
  input->data.f[0] = mq2_value;
  input->data.f[1] = motion_state_encoded;
  input->data.f[2] = fault_dummy;
  input->data.f[3] = recovery_dummy;

  // Run inference
  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("❌ Invoke failed!");
    return;
  }

  // Process output
  int predicted_fault = -1;
  float max_score = -1;
  for (int i = 0; i < output->dims->data[1]; i++) {
    float score = output->data.f[i];
    Serial.printf("Class %d: %.4f\n", i, score);
    if (score > max_score) {
      max_score = score;
      predicted_fault = i;
    }
  }

  Serial.printf("🎯 Predicted Fault: %s (%.4f)\n",
                fault_labels[predicted_fault], max_score);

  // Recovery mapping
  int recovery_type = fault_to_recovery[predicted_fault];
  Serial.printf("🔧 Suggested Recovery: %s\n", recovery_labels[recovery_type]);
  recoveryAction(recovery_type);

  delay(3000);
}
