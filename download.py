from huggingface_hub import hf_hub_download
p = hf_hub_download('distilbert-base-uncased', 'pytorch_model.bin')
print(p)
