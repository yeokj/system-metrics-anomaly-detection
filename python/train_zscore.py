import pandas as pd
import numpy as np

def calculate_baseline(df):
    # Filter out the anomalies
    normal_data = df[df['label'] == 0]

    baseline_params = {}

    metrics = ['latency_roll_mean', 'latency_roll_std', 
               'throughput_roll_mean', 'throughput_roll_std', 
               'error_rate_roll_mean', 'error_rate_roll_std']
    
    for metric in metrics:
        mean_val = normal_data[metric].mean()
        std_val = normal_data[metric].std()

        baseline_params[metric] = {
            'mean': mean_val,
            'std': std_val
        }
    
    return baseline_params