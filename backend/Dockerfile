FROM python:3.12-slim
WORKDIR /app

# Copy the server script and initialize database
COPY run_backend.py .

EXPOSE 8080

# Run the python backend
CMD ["python", "run_backend.py"]
