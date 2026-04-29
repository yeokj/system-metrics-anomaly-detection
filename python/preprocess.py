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
    

def apply_rolling_features(df, window_size=10):
    # Logic to add 'latency_roll_mean', 'latency_roll_std', etc.
    df
    pass

if __name__ == "__main__":
    # The main execution flow: 
    # 1. Load 
    # 2. Transform
    # 3. Save to 'processed_metrics.csv'
    pass