import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier, export_text
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score
from micromlgen import port

# Load CSV data
csv_file = "C:/Users/sohail/Desktop/capstone implemet/data.csv"  # Replace with your actual CSV filename or path
df = pd.read_csv(csv_file)

# Display basic info
print("Dataset shape:", df.shape)
print("Sample data:")
print(df.head())

# Basic cleaning: remove rows with missing values if any
df.dropna(inplace=True)

# Features and target
# Your features are mq2_value and motion_state, target is fault_type
X = df[['mq2_value', 'motion_state']].values
y = df['fault_type'].values

# Check class distribution
print("\nClass distribution (fault_type):")
print(df['fault_type'].value_counts())

# Split data (80% train, 20% test, stratify by fault_type)
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, stratify=y, random_state=42)

# Train Decision Tree Classifier
clf = DecisionTreeClassifier(max_depth=4, random_state=42)  # max_depth can be tuned
clf.fit(X_train, y_train)

# Evaluate on test set
y_pred = clf.predict(X_test)
acc = accuracy_score(y_test, y_pred)
print(f"\nTest accuracy: {acc:.4f}")

print("\nClassification Report:")
print(classification_report(y_test, y_pred))

print("\nConfusion Matrix:")
print(confusion_matrix(y_test, y_pred))

# Show tree rules in text form
tree_rules = export_text(clf, feature_names=['mq2_value', 'motion_state'])
print("\nDecision Tree Rules:")
print(tree_rules)

# Export to C++ code for ESP32 using microMLgen
cpp_code = port(clf, class_names=[str(i) for i in sorted(df['fault_type'].unique())])

cpp_filename = 'fault_detection_model.h'
with open(cpp_filename, 'w') as f:
    f.write('// Auto-generated decision tree model for ESP32\n')
    f.write('#ifndef FAULT_DETECTION_MODEL_H\n#define FAULT_DETECTION_MODEL_H\n\n')
    f.write(cpp_code)
    f.write('\n#endif // FAULT_DETECTION_MODEL_H\n')

print(f"\nC++ model header exported to '{cpp_filename}'. Ready for ESP32 integration.")
