#!/bin/bash

set -e  # Остановить выполнение при ошибке

echo "Сборка multicast_core..."
cd multicast_core/build
make
sudo make install

echo "Сборка основного проекта..."
cd ../../build
make
sudo make install

echo "Сборка завершена успешно."
