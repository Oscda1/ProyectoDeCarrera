
        let currentPhase = 1;
        let deviceType = '';
        let manualNetwork = false;

        let selectedWifiNetwork = '';

        function selectWifiNetwork(networkName) {
            // Deseleccionar todas las opciones
            document.querySelectorAll('.wifi-option').forEach(option => {
                option.classList.remove('selected');
            });

            if( selectedWifiNetwork === networkName) {
                // Si la red ya está seleccionada, deseleccionarla
                selectedWifiNetwork = '';
                return;
            }

            // Seleccionar la opción clickeada
            const selectedOption = Array.from(document.querySelectorAll('.wifi-option')).find(option => option.textContent === networkName);
            if (selectedOption) {
                selectedOption.classList.add('selected');
            }

            // Guardar la red seleccionada
            selectedWifiNetwork = networkName;
        }

        function toggleManualNetwork() {
            if(manualNetwork) {
                // Si ya está visible, ocultar el campo de ingreso manual
                document.getElementById('manualNetwork').classList.add('hidden');
                manualNetwork = false;
                document.getElementById('wifiPanel').classList.remove('hidden');
                document.getElementById('btnManualNetwork').innerText = '¿No ves tu red?';
                // Limpiar el campo de ingreso manual
                document.getElementById('manualNetworkName').value = '';
                selectedWifiNetwork = ''; // Limpiar la red seleccionada
            } else {
                // Mostrar el campo de ingreso manual
                document.getElementById('manualNetwork').classList.remove('hidden');
                manualNetwork = true;
                document.getElementById('wifiPanel').classList.add('hidden');
                // Deseleccionar todas las opciones
                document.querySelectorAll('.wifi-option').forEach(option => {
                    option.classList.remove('selected');
                });
                document.getElementById('btnManualNetwork').innerText = 'Regresar a la lista de redes';
            }
        }

        function skipNetwork(){
            const confirmSkip = confirm(
                '¿Estás seguro de que deseas omitir la configuración de la red WiFi? Si quieres asignarla despues se tendra que volver a configurar el dispositivo.'
            );
            if (!confirmSkip) {
                return; // Detener si el usuario no confirma
            }
            selectWifiNetwork = '';
            currentPhase++;
            updateView();
        }

        function setDeviceType(type) {
            deviceType = type;
        }

        function validatePhase(phase) {
            switch (phase) {
                case 1:
                    const alias = document.getElementById('alias').value.trim();
                    if (!alias) {
                        alert('Por favor, completa el nombre del dispositivo.');
                        return false;
                    }
                    if (!deviceType) {
                        alert('Por favor, selecciona un tipo de dispositivo.');
                        return false;
                    }
                    break;
                case 2:
                    if(deviceType != 'Baliza')
                    {

                            const wifiPassword = document.getElementById('wifiPassword').value.trim();
                            if( !selectedWifiNetwork && !manualNetwork) {
                                alert('Por favor, selecciona o ingresa una red WiFi en la fase 2.');
                                return false;
                            }
                            if (!wifiPassword) {
                                const confirmOpenNetwork = confirm(
                                    'El campo de contraseña está vacío. Esto se interpretará como una red abierta. ¿Deseas continuar?'
                                );
                                if (!confirmOpenNetwork) {
                                    return false; // Detener si el usuario no confirma
                                }
                            }
                        
                    }
                    break;
                case 3:
                    const networkList = document.getElementById('networkList').value.trim();
                    const newNetworkName = document.getElementById('newNetworkName').value.trim();
                    if (!networkList && !newNetworkName) {
                        alert('Por favor, selecciona o ingresa una red en la fase 3.');
                        return false;
                    }
                    break;
                case 4:
                    // No se valida porque es la fase final
                    break;
                default:
                    return true;
            }
            return true;
        }

        function goToPhase(targetPhase) {
            if (targetPhase > currentPhase) {
                for (let phase = currentPhase; phase < targetPhase; phase++) {
                    if (!validatePhase(phase)) {
                        return; // Detener si una fase intermedia no está completa
                    }
                }
            }
            if( deviceType === 'Baliza' && targetPhase === 2){
                if(currentPhase === 1){
                    targetPhase = 3; 
                }else if(currentPhase === 3){
                    targetPhase = 1;
                }else if(currentPhase === 4){
                    targetPhase = 1;
                }
            }
            currentPhase = targetPhase;
            updateView();
        }

        function nextPhase() {
            if (currentPhase < 4) { // Asegurarse de no exceder la fase 4
                if(currentPhase === 2){
                    omitWifi = false;
                }
                if (validatePhase(currentPhase)) {
                    if( currentPhase === 1 && deviceType === 'Baliza'){
                        currentPhase=3;
                        updateView();
                    }else{
                        currentPhase++;
                        updateView();
                    }
                    
                }
            }
        }

        function previousPhase() {
            if (currentPhase > 1) { // Asegurarse de no ir por debajo de la fase 1
                if(currentPhase === 3 && deviceType === 'Baliza'){
                    currentPhase=1;
                }else{
                    currentPhase--;
                }
                updateView();
            }
        }

        function updateView() {

            const phases = document.querySelectorAll('.container > div');
            phases.forEach((phase, index) => {
                if (phase.id.startsWith('phase')) {
                    if (index === currentPhase) {
                        // Mostrar la fase actual con animación de entrada
                        phase.classList.remove('hidden', 'fade-out');
                        phase.classList.add('fade-in');
                    } else if (!phase.classList.contains('hidden')) {
                        // Ocultar la fase anterior con animación de salida
                        phase.classList.remove('fade-in');
                        phase.classList.add('fade-out');
                        setTimeout(() => phase.classList.add('hidden'), 500); // Ocultar después de la animación
                    }
                }
            });
            // Actualizar los indicadores de fase
            document.querySelectorAll('.phase-indicator span').forEach((el, index) => {
                el.classList.toggle('active', index + 1 === currentPhase); // Comparar correctamente con currentPhase
            });

            // Actualizar las vistas de las fases
            document.querySelectorAll('.container > div').forEach((el, index) => {
                if (el.id.startsWith('phase')) { // Asegurarse de que solo las fases sean manipuladas
                    el.classList.toggle('hidden', index !== currentPhase); // Comparar correctamente con currentPhase
                }
            });

            if (currentPhase === 4) {
                const alias = document.getElementById('alias').value;
                const wifiNetwork = selectedWifiNetwork || document.getElementById('manualNetworkName').value;
                const networkList = document.getElementById('networkList').value;
                const newNetworkName = document.getElementById('newNetworkName').value;
                if(deviceType === 'Usuario'){
                    document.getElementById('summary').innerText = ` Tipo de dispositivo: ${deviceType}
                        Alias: ${alias}
                        Red WiFi: ${wifiNetwork}
                        Red seleccionada: ${networkList || newNetworkName}
                    `;
                }else{
                    document.getElementById('summary').innerText = ` Tipo de dispositivo: ${deviceType}
                        Alias: ${alias}
                        Red seleccionada: ${networkList || newNetworkName}
                    `;
                }
            }
        }

        function showCreateNetwork() {
            document.getElementById('createNetwork').classList.toggle('hidden');
        }

        function submitData() {
            // Metodo post con los datos
            const alias = document.getElementById('alias').value;
            const wifiNetwork = selectedWifiNetwork || document.getElementById('manualNetworkName').value;
            const networkList = document.getElementById('networkList').value;
            const newNetworkName = document.getElementById('newNetworkName').value;
            // Enviar los datos con formato DeviceType:alias:wifiNetwork:wifiPassword:networkName
            const data = "RAV:"+ alias + ':' +
                deviceType + ':' +
                (wifiNetwork || 'N/A') + ':' +
                (document.getElementById('wifiPassword').value || 'N/A') + ':' +
                (networkList || newNetworkName);

            console.log('Datos enviados:', data);
            fetch('/submit', {
                method: 'POST',
                headers: {
                    'Content-Type': 'plain/text'
                },
                body: data})
            document.getElementById('btnSubmitData').disabled = true; 
        }

        function toggleCreateNetwork() {
            // Ocultar el selector de redes existentes y mostrar el textbox para crear una nueva red
            document.getElementById('existingNetwork').classList.add('hidden');
            document.getElementById('createNetwork').classList.remove('hidden');
        }

        function toggleExistingNetwork() {
            // Ocultar el textbox para crear una nueva red y mostrar el selector de redes existentes
            document.getElementById('createNetwork').classList.add('hidden');
            document.getElementById('existingNetwork').classList.remove('hidden');
        }
