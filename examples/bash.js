(async () => {
    const {Sandbox} = require('..');
    const sandbox = new Sandbox();
    const result = await sandbox.execute({
        path: '/bin/bash',
        files: {
            0: {inherit: true},
            1: {inherit: true},
            2: {inherit: true},
        }
    });
    console.log(result);
})();
