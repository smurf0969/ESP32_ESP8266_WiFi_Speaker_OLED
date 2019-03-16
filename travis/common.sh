#!/usr/bin/env bash

#set -x # echo on

function print_size_info()
{
    elf_file=$1

    if [ -z "$elf_file" ]; then
        printf "sketch                       data     rodata   bss      text     irom0.text   dram     flash\n"
        return 0
    fi

    elf_name=$(basename $elf_file)
    sketch_name="${elf_name%.*}"
    # echo $sketch_name
    declare -A segments
    while read -a tokens; do
        seg=${tokens[0]}
        seg=${seg//./}
        size=${tokens[1]}
        addr=${tokens[2]}
        if [ "$addr" -eq "$addr" -a "$addr" -ne "0" ] 2>/dev/null; then
            segments[$seg]=$size
        fi


    done < <(xtensa-lx106-elf-size --format=sysv $elf_file)

    total_ram=$((${segments[data]} + ${segments[rodata]} + ${segments[bss]}))
    total_flash=$((${segments[data]} + ${segments[rodata]} + ${segments[text]} + ${segments[irom0text]}))

    printf "%-28s %-8d %-8d %-8d %-8d %-8d     %-8d %-8d\n" $sketch_name ${segments[data]} ${segments[rodata]} ${segments[bss]} ${segments[text]} ${segments[irom0text]} $total_ram $total_flash
    return 0
}

function build_sketches()
{
    set +e
    local arduino=$1
    local srcpath=$2
    local build_arg=$3
    local build_dir=build.tmp
    mkdir -p $build_dir
    local build_cmd="python $arduino/$BUILD_PY -p $PWD/$build_dir $build_arg "
    local sketches=$(find $srcpath -name *.ino)
    print_size_info >size.log
    export ARDUINO_IDE_PATH=$arduino
    for sketch in $sketches; do
        rm -rf $build_dir/*
        local sketchdir=$(dirname $sketch)
        local sketchdirname=$(basename $sketchdir)
        local sketchname=$(basename $sketch)
        if [[ "${sketchdirname}.ino" != "$sketchname" ]]; then
            echo "Skipping $sketch, beacause it is not the main sketch file";
            continue
        fi;
        if [[ -f "$sketchdir/.test.skip" ]]; then
            echo -e "\n ------------ Skipping $sketch ------------ \n";
            continue
        fi
        echo -e "\n ------------ Building $sketch ------------ \n";
        # $arduino --verify $sketch;
        echo "$build_cmd $sketch"
        time ($build_cmd $sketch >build.log)
        local result=$?
        if [ $result -ne 0 ]; then
            echo "Build failed ($1)"
            echo "Build log:"
            cat build.log
            set -e
            return $result
        fi
        rm build.log
        #print_size_info $build_dir/*.elf >>size.log
    done
    set -e
}

function install_libraries()
{
    mkdir -p $HOME/Arduino/libraries
    #Copy to sketch or library folder
    cp -a $TRAVIS_BUILD_DIR $HOME/Arduino/
    git clone https://github.com/greiman/SdFat $HOME/Arduino/libraries/SdFat
    git clone https://github.com/greiman/SdFs $HOME/Arduino/libraries/SdFs
    git clone https://github.com/earlephilhower/ESP8266Audio $HOME/Arduino/libraries/ESP8266Audio
    git clone https://github.com/Gianbacchio/ESP8266_Spiram $HOME/Arduino/libraries/ESP8266_Spiram
    git clone https://github.com/smurf0969/WiFiConnect $HOME/Arduino/libraries/WiFiConnect
    git clone https://github.com/bblanchon/ArduinoJson $HOME/Arduino/libraries/ArduinoJson
    git clone --branch=Allow-overriding-default-font https://github.com/smurf0969/esp8266-oled-ssd1306 $HOME/Arduino/libraries/esp8266-oled-ssd1306
     # Following libs are not to be tested, just used.
    rm -rf $HOME/Arduino/libraries/SdFat/examples
    rm -rf $HOME/Arduino/libraries/SdFs/examples
    rm -rf $HOME/Arduino/libraries/ESP8266Audio/examples
    rm -rf $HOME/Arduino/libraries/ESP8266_Spiram/examples
    rm -rf $HOME/Arduino/libraries/WiFiConnect/examples
    rm -rf $HOME/Arduino/libraries/ArduinoJson/examples
    rm -rf $HOME/Arduino/libraries/esp8266-oled-ssd1306/examples
}

function install_ide()
{
    local ide_path=$1
    local core_path=$2
    # if .travis.yml does not set version
    if [ -z $ARDUINO_IDE_VERSION ]; then
    export ARDUINO_IDE_VERSION="1.8.7"
    echo "NOTE: YOUR .TRAVIS.YML DOES NOT SPECIFY ARDUINO IDE VERSION, USING $ARDUINO_IDE_VERSION"
    fi

    # if newer version is requested
    if [ ! -f $ide_path/$ARDUINO_IDE_VERSION ] && [ -f $ide_path/arduino ]; then
    echo -n "DIFFERENT VERSION OF ARDUINO IDE REQUESTED: "
    shopt -s extglob
    cd $ide_path
    rm -rf *
    if [ $? -ne 0 ]; then echo -e "\xe2\x9c\x96"; else echo -e "\xe2\x9c\x93"; fi
    cd $OLDPWD
    fi

    # if not already cached, download and install arduino IDE
    echo -n "ARDUINO IDE STATUS: "
    if [ ! -f $ide_path/arduino ]; then
    echo -n "DOWNLOADING: "
    wget --quiet https://downloads.arduino.cc/arduino-$ARDUINO_IDE_VERSION-linux64.tar.xz
    if [ $? -ne 0 ]; then echo -e "\xe2\x9c\x96"; else echo -e "\xe2\x9c\x93"; fi
    echo -n "UNPACKING ARDUINO IDE: "
    [ ! -d $ide_path/ ] && mkdir $ide_path
    tar xf arduino-$ARDUINO_IDE_VERSION-linux64.tar.xz -C $ide_path/ --strip-components=1
    if [ $? -ne 0 ]; then echo -e "\xe2\x9c\x96"; else echo -e "\xe2\x9c\x93"; fi
    touch $ide_path/$ARDUINO_IDE_VERSION
    else
    echo -n "CACHED: "
    echo -e "\xe2\x9c\x93"
    fi
    echo -e "Installing Hardware"
    mkdir -p $ide_path/hardware
    cd $ide_path/hardware
    rm -rf esp8266com
    mkdir esp8266com
    cd esp8266com
    git clone https://github.com/esp8266/Arduino.git esp8266
    cd esp8266
    git submodule update --init
    cd ..
    pushd esp8266/tools
    python get.py
    export PATH="$ide_path:$ide_path/hardware/esp8266com/esp8266/tools/xtensa-lx106-elf/bin:$PATH"
    popd
    cd ..
    rm -rf espressif
    mkdir espressif
    cd espressif
    git clone https://github.com/espressif/arduino-esp32 esp32
    pushd esp32/tools
    python get.py
    export PATH="$ide_path:$ide_path/hardware/espressif/esp32/tools/xtensa-esp32-elf/bin:$PATH"
    popd
}

function install_arduino()
{
    # Install Arduino IDE and required libraries
    echo -e "travis_fold:start:sketch_test_env_prepare"
    cd $TRAVIS_BUILD_DIR
    install_ide $HOME/arduino_ide $TRAVIS_BUILD_DIR
    which arduino
    cd $TRAVIS_BUILD_DIR
    install_libraries
    echo -e "travis_fold:end:sketch_test_env_prepare"
}

function build_sketches_with_arduino()
{
    # Compile sketches
    echo -e "travis_fold:start:sketch_test"
#    build_sketches $HOME/arduino_ide $TRAVIS_BUILD_DIR/libraries "-l $HOME/Arduino/libraries"
    #library
    #build_sketches $HOME/arduino_ide $HOME/Arduino/libraries "-l $HOME/Arduino/libraries"
    #sketch
    build_sketches $HOME/arduino_ide $HOME/Arduino "-l $HOME/Arduino/libraries"
    echo -e "travis_fold:end:sketch_test"

    # Generate size report
    echo -e "travis_fold:start:size_report"
    cat size.log
    echo -e "travis_fold:end:size_report"
}

set -e

if [ "$BUILD_TYPE" = "build_esp8266" ]; then
    export BUILD_PY="hardware/esp8266com/esp8266/tools/build.py -b nodemcuv2 -s 4M1M -v -k "
    install_arduino
    build_sketches_with_arduino
elif [ "$BUILD_TYPE" = "build_esp32" ]; then
    export BUILD_PY="hardware/espressif/esp32/tools/build.py -b esp32 -v -k  "
    install_arduino
    build_sketches_with_arduino
fi
