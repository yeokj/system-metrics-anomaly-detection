import pandas as pd
import numpy as np
import os

# Logic to load CSV and handle timestamps
def load_simulation_data(file_path):
    # Check if the file exists before trying to read it
    if not os.path.exists(file_path):
        print(f"Error: {file_path} not found. Run the C++ simulator first.")
        return None
    
    try:
        # 1. Load the CSV
        df = pd.read_csv(file_path)

        # 2. Convert timestamp column (C++ is sending seconds)
        df['timestamp'] = pd.to_datetime(df['timestamp'], unit='s')
        
        # 3. Set the timestamp as the index
        df = df.set_index('timestamp').sort_index()
        
        print(f"Successfully loaded {len(df)} rows of simulation data.")
        return df
    
    except Exception as e:
        print(f"An unexpected error has occured: {e}")
        return None
    
# Logic to add 'latency_roll_mean', 'latency_roll_std', etc.
def apply_rolling_features(df, window_size=10):
    metrics = ['latency', 'throughput', 'error_rate']

    for metric in metrics:
        df[f'{metric}_roll_mean'] = df[metric].rolling(window=window_size).mean()
        df[f'{metric}_roll_std'] = df[metric].rolling(window=window_size).std()

    df = df.dropna()

    return df

# The main execution flow
if __name__ == "__main__":
    input_path = 'data/raw_metrics.csv'
    output_path = 'data/processed_metrics.csv'
    
    # 1. Load the data
    raw_df = load_simulation_data(input_path)
    
    if raw_df is not None:
        # 2. Transform the data
        processed_df = apply_rolling_features(raw_df, window_size=10)
        
        # 3. Save to 'processed_metrics.csv'
        processed_df.to_csv(output_path)
        print(f"Preprocessing complete. Saved processed data to {output_path}")
        
        # Research Check: Peek at the first few rows to verify the new columns
        print(processed_df.head())