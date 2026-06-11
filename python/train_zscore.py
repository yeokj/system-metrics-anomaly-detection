import pandas as pd
import numpy as np

def calculate_baseline(df):
    # Filter out the anomalies
    normal_data = df[df['label'] == 0]

    baseline_params = {}

    metrics = ['latency', 'throughput', 'error_rate']
    
    for metric in metrics:
        mean_val = normal_data[metric].mean()
        std_val = normal_data[metric].std()

        baseline_params[f"{metric}_roll_mean"] = {
            'mean': mean_val,
            'std': std_val
        }
        baseline_params[f"{metric}_roll_std"] = {
            'mean': mean_val,
            'std': std_val
        }
    
    return baseline_params