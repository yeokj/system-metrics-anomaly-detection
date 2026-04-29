import json
import pandas as pd
from train_zscore import calculate_baseline

def save_to_json(params, output_path):
    try:
        with open(output_path, 'w') as f:
            json.dump(params, f, indent=4)
        print(f"Success! Model parameters written to {output_path}")
    except Exception as e:
        print(f"Failed to export: {e}")

if __name__ == "__main__":
    # 1. Load the processed data
    df = pd.read_csv('data/processed_metrics.csv')
    
    # 2. Transfer the data to a dictionary
    params = calculate_baseline(df)
    
    # 3. Export
    save_to_json(params, 'data/model_params.json')