	/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   exam.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: naastrak <naastrak@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/02/23 14:50:46 by naastrak          #+#    #+#             */
/*   Updated: 2026/02/23 17:05:57 by naastrak         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <unistd.h>//Unix 系统调用函数。fork execve write close pipe dup dup2
#include <sys/wait.h>//waitpid()等待子进程结束
#include <string.h>//strcmp()
#include <stdlib.h>//exit()
//打印错误信息到 stderr  ("error: cannot execute ", av[0])     例如  error: cannot execute /bin/ls
void	ft_puterror(char *str, char *arg)
{
	while (*str)
		write(2, str++, 1);//循环打印 str, 2是stderr
	if (arg)//arg 是否存在,如果有参数，就打印。
		while (*arg)//循环打印 arg。
			write(2, arg++, 1);//逐字符打印 arg。
	write(2, "\n", 1);//换行符
}
//严重错误
void fatal_error(void)
{
	ft_puterror("error: fatal", NULL);
	exit(1);//结束程序,1 = error exit
}
//执行一个命令   参数列表   命令长度   输入fd    环境变量
void ft_exec(char **av, int i, int tmp_fd, char **env)//
{								//tmp_fd 始终代表“当前命令应该从哪里读取输入”
	av[i] = NULL;//把命令数组截断,因为execve 需要一个以 NULL 结尾的参数数组作第二个参数。
	if (dup2(tmp_fd, STDIN_FILENO) == -1)//=-1说明dup2调用失败  //第一条命令：它从标准输入（键盘）读，所以初始化时 tmp_fd = dup(STDIN_FILENO)。管道后的命令：如果前一个命令有 |，那么当前命令就不能从键盘读了，得从上一个管道的读端读。
		fatal_error();//如果 dup2 失败就打印 fatal error //execve 执行的新程序（比如 grep）默认只从 STDIN_FILENO（文件描述符 0）读取数据。它并不知道你程序里定义的 tmp_fd 是什么。
	close(tmp_fd);//关闭旧 fd,现在 tmp_fd 就没用了因为：stdin 已经复制好了,程序读取输入时会用stdin 而不会再用tmp_fd
	execve(av[0], av, env);//执行程序,成功的话就会exit //av[0]是程序名,指明可执行文件的路径。av是传递给程序的命令行参数数组,是要干的活, env是在什么环境下干活？
	ft_puterror("error: cannot execute ", av[0]); //execve 失败才会到这里,如果 execve 失败就打印
	exit(1);//error exit
}

//假设运行 ./microshell /bin/ls "|" /usr/bin/grep microshell ";" /bin/echo hello
//env是操作系统传给程序的环境变量列表,当程序启动时，系统会传入ac av env这些东西
int	main(int ac, char **av, char **env)
{
	int		i = 1;//以此跳过av[0]			
	int		fd[2];//这里fd[2]是一个长度为2的数组,fd[0] 读端 fd[1] 写端, 后续系统会把两个文件描述符fd[0] 和fd[1] 放进创建的pipe
	int		tmp_fd;//当前命令的输入来源
/*
tmp_fd 的意义：它像是一个接力棒。最开始，接力棒指向键盘（标准输入）。如果后面有管道，它就会指向管道的出口。
为什么 dup？ 因为我们要不断修改 tmp_fd，所以先复制一份，保证我们手里始终有一个有效的“起始输入源”。
*/
	tmp_fd = dup(STDIN_FILENO);//复制标准输入，因为后面管道处理时会不断修改输入来源，需要保存一份原始的标准输入备用. 默认情况下，fd=0 连接的是键盘，程序从这里读取用户输入。
	if (tmp_fd == -1)
		fatal_error();
	//循环遍历所有参数，i < ac 防止越界，av[i] 防止空指针
	while (i < ac && av[i])
	{
		char	**cmd_start = &av[i];//cmd_start是指向指针的指针,让cmd_start 指向当前命令av[i]
		int		j = 0;//j用来遍历cmd_start
		//三个都为真就j++, strcmp为真=两个字符串不一样;//跳过参数,直到遇到 ; 或 | 或结尾
		while (cmd_start[j] && strcmp(cmd_start[j], ";") && strcmp(cmd_start[j], "|"))
			j++;

		if (j > 0)//确实找到了一条命令
		{
			if (strcmp(cmd_start[0], "cd") == 0)//如果当前命令是 cd。cd 比较特殊，不能用 execve 执行，因为 cd 需要改变当前进程的目录，如果放到子进程里执行，子进程退出后父进程的目录不会变，所以单独处理。
			{
				if (j != 2)//如果命令数不是2. cd 只接受一个参数，所以 j 必须等于 2（cd 本身 + 目标路径）。不是2就报错。
					ft_puterror("error: cd: bad arguments", NULL);
				else if (chdir(cmd_start[1]) != 0)//如果cd后的参数不是一个合法路径,chdir调用失败.cmd_start[1]是av[2],也就是cd后边那个参数得是一个路径比如/home/user
					ft_puterror("error: cd: cannot change directory to ", cmd_start[1]);
			}//如果cmd_start[j] 是 NULL（到结尾了），或者是 ";". 这两种情况都说明当前命令不需要管道，直接执行就行。
			else if (cmd_start[j] == NULL || strcmp(cmd_start[j], ";") == 0)
			{   //fork() 创建一个子进程。调用之后，程序会分裂成两个进程，父进程和子进程从这里开始同时运行。
				pid_t pid = fork();//pid_t 是进程ID的类型，本质就是一个整数。pid是执行fork后的返回值
				if (pid == -1)//返回值是-1,fork失败,直接退出
					fatal_error();
				if (pid == 0)//返回值是0,说明是子进程，子进程调用 ft_exec 执行命令，然后就结束了，不会再往下走。
					ft_exec(cmd_start, j, tmp_fd, env);
				//父进程逻辑
				close(tmp_fd);//因为tmp_fd已经给了子进程,父进程不需要了.//父进程 pid 如果是正数，跳过前两个if，走到了这里.这里父进程关闭 tmp_fd，因为子进程已经接管了，父进程自己不再需要它。不关的话会造成 fd 泄漏。//作为导演，你手里这份旧的 tmp_fd 就没用了。记住： 只要你手里还攥着这个 fd，系统就认为这根“管道”还没用完。为了不让后台堆满没用的道具（FD 泄漏），你必须把它扔了。//只有当所有指向管道写端（fd[1]）的描述符都关闭时，读端（fd[0]）才会读到 EOF（结束标志）。
				while (waitpid(-1, NULL, 0) > 0)//如果是最后一条命令或者后面是分号，说明这个任务是独立的。父进程等它执行完，然后把输入源（接力棒）重置回键盘，准备开始全新的下一组任务。
					;//>0的时候说明还有子进程, 没有子进程waitpid就会返回 -1，循环停止//舞台上可能还有好几个替身（子进程）在各忙各的。
				tmp_fd = dup(STDIN_FILENO);//子进程执行完了，重新复制一份标准输入，为下一条命令准备好输入来源。
				if (tmp_fd == -1) //之前：tmp_fd 可能指向某个管道的出口（比如 grep 正在吸前一个人的数据）。现在：遇到了 ;，说明这一组管道戏演完了！下一场：下一场戏（比如 ; echo hello）是一个全新的开始，它应该从键盘（标准输入）重新读数据。
					fatal_error();
			} //这块是管道处理
			else if (strcmp(cmd_start[j], "|") == 0)//如果j停在的位置是 |
			{
				pid_t pid;

				if (pipe(fd) == -1)//pipe调用返回-1说明失败,直接退出
					fatal_error();//一定要先 pipe 然后 fork, 要先买管子, fd[0]是接水端，从这里读数据; fd[1]是出水端，往这里写数据
				pid = fork();//用fork创建子进程
				if (pid == -1)//返回-1说明fork调用失败,退出
					fatal_error();
				if (pid == 0)//子进程里
				{	// 第一步：把输出口接到管道的“出水端”,因为execve非常“死脑筋”，它只认标准输出。它根本不知道你程序里定义的变量 fd[1] 是什么东西。
					if (dup2(fd[1], STDOUT_FILENO) == -1)//把标准输出重定向到管道写端，命令打印的内容不再输出到屏幕，而是写入管道,失败的话退出
						fatal_error();
					close(fd[0]);//第二步：我是灌水的，要出口干嘛？关掉。
					close(fd[1]);//我已经把灌水端接到我的 STDOUT 上了，手里的原件不需要了,关掉
					ft_exec(cmd_start, j, tmp_fd, env);// 第三步：开始干活（变身成真正的命令）
				}
				close(fd[1]);//键！水管工（父进程）必须放下灌水端。如果不关，下游的人永远以为还有水要来，会死等的！
				close(tmp_fd);//上一组任务传过来的那个“出口”已经交给子进程去吸了，我手里这个没用了，关掉。
				tmp_fd = fd[0];//把管道接水端保存为新的 tmp_fd。
			}
		}
		i += j;//i 往前跳 j 个位置，跳过命令行的所有参数。
		if (av[i])//跳过分隔符（| 或 ;）,因为不需要处理
			i++;//这样 i 就会指向 下一条命令的开始位置。
	}
	close(tmp_fd);
	return (0);
}

/*
dup2 用来复制文件描述符，让两个 fd 指向同一个地方。

函数原型：
cint dup2(int oldfd, int newfd);
效果：让 newfd 指向 oldfd 所指向的地方。之后对 newfd 的读写，实际上就是对 oldfd 的读写。

比如:
int fd = open("input.txt", O_RDONLY);
dup2(fd, STDIN_FILENO);
现在程序读 stdin，实际上读的是 input.txt
FILENO是File Number 的缩写，也就是文件编号。
STDIN_FILENO    Standard Input File Number		0
STDOUT_FILENO   Standard Output File Number		1
STDERR_FILENO   Standard Error File Number		2


env 是很多字符串，例如：
PATH=/usr/bin:/bin
HOME=/home/user
USER=student
LANG=en_US.UTF-8
PWD=/home/user/project

env[0] = "PATH=/usr/bin:/bin"
env[1] = "HOME=/home/user"
env[2] = "USER=student"
...
env[n] = NULL


; — 这条命令结束，下一条独立执行
| — 这条命令结束，输出传给下一条命令

strcmp 返回 0   → 两个字符串相同 → 假
strcmp 返回非0  → 两个字符串不同 → 真

chdir 是改变当前目录的系统调用，全称 change directory
函数原型: int chdir(const char *path);
参数是要跳转的目录路径:成功返回 0;失败返回 -1（目录不存在、没有权限等）

fork() 返回值有三种情况：
-1	 →		fork 失败
0	 →		当前是子进程
正数  →		 当前是父进程，返回值是子进程的pid
*/
