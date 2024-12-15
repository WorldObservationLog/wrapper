FROM ubuntu:latest

WORKDIR /app
COPY . /app
ENV args ""

CMD ["bash", "-c", "./wrapper ${args}"]

EXPOSE 10020 20020