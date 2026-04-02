/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   examm.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: xzhen <xzhen@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/04/02 13:39:36 by xzhen             #+#    #+#             */
/*   Updated: 2026/04/02 15:16:50 by xzhen            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "examm.h"

void ft_puterror(char *str, char *arg)
{
    while (*str)
        write(2, str++, 1);
    if(arg)
    {
        while(*arg)
            write(2, arg++, 1);
    }
    write(2, "\n", 1);
}

void fatal_error(void)
{
    ft_puterror("error: fatal", NULL);
    exit(1);    
}

void ft_exec(char **av, int i, int tmp_fd, char **env)
{
    av[i] = NULL;
    if (dup2(tmp_fd, STDIN_FILENO) == -1)
        fatal_error();
    execve(av[0], av, env);
    ft_puterror("cannot ex ", av[0]);
    exit(1);
}

int main(int ac, char **av, char **env)
{
    int fd[2];
    int tmp_fd;
    int i = 1;
    
    tmp_fd = dup(STDIN_FILENO);
    if(tmp_fd == -1)
        fatal_error();
    while(i < ac && av[i])
    {
        int j = 0;
        char **cmd_start = &av[i];
        
        while (cmd_start[j] && strcmp(cmd_start[j], ";") && strcmp(cmd_start[j], "|"))
            j++;
        
        if(strcmp(cmd_start[0], "cd") == 0)
        {
            if ( j!= 2)
                ft_puterror("bad arguments", NULL);
            else if(chdir(cmd_start[1]) != 0)
                ft_puterror("cannot change the directory to ", cmd_start[1]);
        }
        else if(cmd_start[j] == NULL || strcmp(cmd_start[j], ";") == 0)
        {
            pid_t pid = fork();
            if (pid == -1)
                fatal_error();
            if(pid == 0)
                ft_exec(cmd_start, j, tmp_fd, env);
            close(tmp_fd);
            while (waitpid(-1, NULL, 0) > 0)
                ;
            tmp_fd = dup(STDIN_FILENO);
            if(tmp_fd == -1)
                fatal_error();
        }
        else if(strcmp(cmd_start[j], "|") == 0)
        {
            pid_t pid;
            if(pipe(fd) == -1)
                fatal_error();
            pid = fork();
            if (pid == -1)
                fatal_error();
            if(pid == 0)
            {
                if(dup2(fd[1], STDOUT_FILENO) == -1)
                    fatal_error();
                close(fd[0]);
                close(fd[1]);
                ft_exec(cmd_start, j, tmp_fd, env);
            }
            close(fd[1]);
            close(tmp_fd);
            tmp_fd = fd[0];
        }
        i += j;
        if(av[i])
            i++;
    }
    close(tmp_fd);
    return(0);
}
